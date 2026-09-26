#include "a2e/agent.hpp"

#include "a2e/scene.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace a2e {

bool MoveToAction::validate(const ActionContext& context) const {
    if (!context.self.get_component<NavigationAgent>()) return false;
    if (!context.grid) return true;
    const auto cell = context.grid->world_to_cell(x_, y_);
    return cell && !context.grid->is_blocked(*cell);
}

void MoveToAction::start(ActionContext& context) {
    auto* navigation = context.self.get_component<NavigationAgent>();
    navigation->set_destination(x_, y_);
    // Clear any Arrived/Unreachable status left over from the previous destination.
    navigation->status = NavigationStatus::Idle;
}

ActionStatus MoveToAction::update(ActionContext& context, double) {
    auto* navigation = context.self.get_component<NavigationAgent>();
    if (!navigation) return ActionStatus::Failed;
    const double dx = context.self.transform().x - x_;
    const double dy = context.self.transform().y - y_;
    if (dx * dx + dy * dy <= tolerance_ * tolerance_ || navigation->status == NavigationStatus::Arrived) {
        return ActionStatus::Succeeded;
    }
    if (navigation->status == NavigationStatus::Unreachable) return ActionStatus::Failed;
    return ActionStatus::Running;
}

void MoveToAction::stop(ActionContext& context) {
    if (auto* navigation = context.self.get_component<NavigationAgent>()) navigation->stop();
}

bool MoveToAction::same_as(const AgentAction& other) const {
    const auto* move = dynamic_cast<const MoveToAction*>(&other);
    if (!move) return false;
    const double dx = move->x_ - x_;
    const double dy = move->y_ - y_;
    return dx * dx + dy * dy <= tolerance_ * tolerance_;
}

ActionStatus WaitAction::update(ActionContext&, double delta_seconds) {
    elapsed_ += delta_seconds;
    return elapsed_ >= seconds_ ? ActionStatus::Succeeded : ActionStatus::Running;
}

RulePolicy& RulePolicy::add_rule(std::string name, Condition condition, Factory factory) {
    if (!factory) throw std::invalid_argument("rule needs an action factory");
    rules_.push_back({std::move(name), std::move(condition), std::move(factory)});
    return *this;
}

std::shared_ptr<AgentAction> RulePolicy::decide(const DecisionContext& context) {
    for (const auto& rule : rules_) {
        if (rule.condition && !rule.condition(context)) continue;
        if (auto action = rule.factory(context)) {
            last_rule_ = rule.name;
            return action;
        }
    }
    last_rule_.clear();
    return nullptr;
}

void Agent::add_sensor(std::shared_ptr<Sensor> sensor) {
    if (!sensor) throw std::invalid_argument("sensor cannot be null");
    sensors_.push_back(std::move(sensor));
}

Goal& Agent::add_goal(Goal goal) {
    if (goal.name.empty()) throw std::invalid_argument("goal name cannot be empty");
    if (auto* existing = this->goal(goal.name)) {
        *existing = std::move(goal);
        return *existing;
    }
    goals_.push_back(std::move(goal));
    return goals_.back();
}

Goal* Agent::goal(const std::string& name) {
    for (auto& goal : goals_) if (goal.name == name) return &goal;
    return nullptr;
}

const Goal* Agent::goal(const std::string& name) const {
    for (const auto& goal : goals_) if (goal.name == name) return &goal;
    return nullptr;
}

const Goal* Agent::top_goal() const {
    const Goal* best = nullptr;
    for (const auto& goal : goals_) {
        if (!goal.completed && (!best || goal.priority > best->priority)) best = &goal;
    }
    return best;
}

void Agent::update_goals(double time) {
    for (auto& goal : goals_) {
        if (goal.evaluate && !goal.completed) goal.priority = goal.evaluate(*this, time);
    }
}

AgentSystem::AgentSystem(EventBus* events, const NavigationGrid* grid) : events_(events), grid_(grid) {
    if (events_) {
        sound_subscription_ = events_->subscribe<SoundProducedEvent>(
            [this](const SoundProducedEvent& sound) { sounds_.push_back(sound); });
    }
}

AgentSystem::~AgentSystem() { sound_subscription_.unsubscribe(); }

void AgentSystem::publish_action(std::uint64_t agent, const AgentAction& action, AgentActionEvent::Kind kind) {
    if (events_) events_->publish(AgentActionEvent{agent, action.name(), kind});
}

void AgentSystem::update(Scene& scene, const InputState&, double delta_seconds) {
    time_ += delta_seconds;
    for (const auto& entity : scene.entities()) {
        if (!entity->active()) continue;
        auto* agent = entity->get_component<Agent>();
        if (!agent) continue;

        // Facing follows movement so vision looks where the agent is going.
        const auto& transform = entity->transform();
        if (agent->has_last_position_) {
            const double dx = transform.x - agent->last_x_;
            const double dy = transform.y - agent->last_y_;
            const double length = std::sqrt(dx * dx + dy * dy);
            if (length > 1e-6) {
                agent->facing_x = dx / length;
                agent->facing_y = dy / length;
            }
        }
        agent->has_last_position_ = true;
        agent->last_x_ = transform.x;
        agent->last_y_ = transform.y;

        // Perceive, then remember. Detection events fire only for sources the agent had forgotten.
        agent->perceived_.clear();
        const SensorContext sensing{scene, *entity, agent->facing_x, agent->facing_y, time_, sounds_, grid_};
        for (const auto& sensor : agent->sensors_) sensor->sense(sensing, agent->perceived_);
        for (const auto& observation : agent->perceived_) {
            const bool known = observation.source != 0 &&
                               !agent->memory.recall([&observation](const MemoryRecord& record) {
                                   return record.observation.source == observation.source &&
                                          record.observation.kind == observation.kind;
                               }).empty();
            agent->memory.remember(observation);
            if (!known && events_) events_->publish(AgentDetectedEvent{entity->id(), observation});
        }
        agent->memory.update(delta_seconds);

        agent->update_goals(time_);
        for (const auto& goal : agent->goals_) {
            auto& reported = agent->reported_goals_;
            const bool was_reported = std::find(reported.begin(), reported.end(), goal.name) != reported.end();
            if (goal.completed && !was_reported) {
                reported.push_back(goal.name);
                if (events_) events_->publish(GoalCompletedEvent{entity->id(), goal.name});
            } else if (!goal.completed && was_reported) {
                reported.erase(std::remove(reported.begin(), reported.end(), goal.name), reported.end());
            }
        }

        ActionContext acting{scene, *entity, *agent, time_, grid_};
        const auto accepted = [this, &acting](const AgentAction& action) {
            return action.validate(acting) && (!validator_ || validator_(acting, action));
        };

        agent->decision_timer_ -= delta_seconds;
        if (agent->policy_ && (!agent->action_ || agent->decision_timer_ <= 0.0)) {
            agent->decision_timer_ = agent->decision_interval;
            ++agent->stats_.decisions;
            const DecisionContext deciding{scene, *entity, *agent, time_};
            auto proposal = agent->policy_->decide(deciding);
            if (proposal && !(agent->action_ && agent->action_->same_as(*proposal))) {
                if (!accepted(*proposal)) {
                    ++agent->stats_.actions_rejected;
                    publish_action(entity->id(), *proposal, AgentActionEvent::Kind::Rejected);
                } else {
                    if (agent->action_) {
                        agent->action_->stop(acting);
                        publish_action(entity->id(), *agent->action_, AgentActionEvent::Kind::Interrupted);
                    }
                    agent->action_ = std::move(proposal);
                    agent->action_->start(acting);
                    ++agent->stats_.actions_started;
                    publish_action(entity->id(), *agent->action_, AgentActionEvent::Kind::Started);
                }
            }
        }

        if (agent->action_) {
            const auto status = agent->action_->update(acting, delta_seconds);
            if (status != ActionStatus::Running) {
                const auto finished = std::move(agent->action_);
                finished->stop(acting);
                const bool succeeded = status == ActionStatus::Succeeded;
                ++(succeeded ? agent->stats_.actions_succeeded : agent->stats_.actions_failed);
                publish_action(entity->id(), *finished,
                               succeeded ? AgentActionEvent::Kind::Succeeded : AgentActionEvent::Kind::Failed);
                // Decide again next update instead of idling for a full interval.
                agent->decision_timer_ = 0.0;
            }
        }
    }
    sounds_.clear();
}

} // namespace a2e
