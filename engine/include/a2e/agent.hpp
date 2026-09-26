#pragma once

#include "a2e/agent_memory.hpp"
#include "a2e/events.hpp"
#include "a2e/navigation.hpp"
#include "a2e/perception.hpp"
#include "a2e/update_system.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace a2e {

class Agent;

// A goal the agent may pursue. Priority can be recomputed each update from the agent's own state.
struct Goal {
    std::string name;
    double priority = 0.0;
    bool completed = false;
    std::function<double(const Agent& agent, double time)> evaluate;
};

struct ActionContext {
    Scene& scene;
    Entity& self;
    Agent& agent;
    double time;
    const NavigationGrid* grid;
};

enum class ActionStatus { Running, Succeeded, Failed };

// Something an agent does to the world. Actions are validated before they start so agents cannot
// perform actions the environment does not allow.
class AgentAction {
public:
    virtual ~AgentAction() = default;
    virtual std::string name() const = 0;
    virtual bool validate(const ActionContext&) const { return true; }
    virtual void start(ActionContext&) {}
    virtual ActionStatus update(ActionContext& context, double delta_seconds) = 0;
    virtual void stop(ActionContext&) {}
    // Used when a policy proposes an action while another runs; equivalent actions are not restarted.
    virtual bool same_as(const AgentAction& other) const { return name() == other.name(); }
};

// Walks to a point through the entity's NavigationAgent. Invalid without one or when the target is blocked.
class MoveToAction final : public AgentAction {
public:
    MoveToAction(double x, double y, double tolerance = 16.0) : x_(x), y_(y), tolerance_(tolerance) {}
    std::string name() const override { return "move_to"; }
    bool validate(const ActionContext& context) const override;
    void start(ActionContext& context) override;
    ActionStatus update(ActionContext& context, double delta_seconds) override;
    void stop(ActionContext& context) override;
    bool same_as(const AgentAction& other) const override;
    double x() const { return x_; }
    double y() const { return y_; }

private:
    double x_;
    double y_;
    double tolerance_;
};

class WaitAction final : public AgentAction {
public:
    explicit WaitAction(double seconds) : seconds_(seconds) {}
    std::string name() const override { return "wait"; }
    ActionStatus update(ActionContext& context, double delta_seconds) override;

private:
    double seconds_;
    double elapsed_ = 0.0;
};

struct DecisionContext {
    const Scene& scene;
    const Entity& self;
    const Agent& agent;
    double time;
};

// Chooses what the agent should do next. Returning nullptr means "keep doing the current action, or idle".
class DecisionPolicy {
public:
    virtual ~DecisionPolicy() = default;
    virtual std::shared_ptr<AgentAction> decide(const DecisionContext& context) = 0;
};

// Ordered condition -> action rules; the first matching rule that produces an action wins.
class RulePolicy final : public DecisionPolicy {
public:
    using Condition = std::function<bool(const DecisionContext&)>;
    using Factory = std::function<std::shared_ptr<AgentAction>(const DecisionContext&)>;

    RulePolicy& add_rule(std::string name, Condition condition, Factory factory);
    std::shared_ptr<AgentAction> decide(const DecisionContext& context) override;
    const std::string& last_rule() const { return last_rule_; }

private:
    struct Rule {
        std::string name;
        Condition condition;
        Factory factory;
    };
    std::vector<Rule> rules_;
    std::string last_rule_;
};

struct AgentStats {
    std::uint64_t decisions = 0;
    std::uint64_t actions_started = 0;
    std::uint64_t actions_succeeded = 0;
    std::uint64_t actions_failed = 0;
    std::uint64_t actions_rejected = 0;
};

// Entity component composing perception, memory, beliefs, goals, and a decision policy.
// Polymorphic parts are shared so the component stays copyable; copies share sensors and policy.
class Agent {
public:
    void add_sensor(std::shared_ptr<Sensor> sensor);
    void set_policy(std::shared_ptr<DecisionPolicy> policy) { policy_ = std::move(policy); }
    Goal& add_goal(Goal goal);
    Goal* goal(const std::string& name);
    const Goal* goal(const std::string& name) const;
    // Highest-priority incomplete goal, or nullptr.
    const Goal* top_goal() const;
    void update_goals(double time);

    AgentMemory memory;
    Beliefs beliefs;
    double decision_interval = 0.25;
    double facing_x = 1.0;
    double facing_y = 0.0;

    const std::vector<std::shared_ptr<Sensor>>& sensors() const { return sensors_; }
    DecisionPolicy* policy() const { return policy_.get(); }
    const std::vector<Goal>& goals() const { return goals_; }
    const AgentAction* current_action() const { return action_.get(); }
    // Observations produced by the most recent perception step.
    const std::vector<Observation>& perceived() const { return perceived_; }
    const AgentStats& stats() const { return stats_; }

private:
    friend class AgentSystem;

    std::vector<std::shared_ptr<Sensor>> sensors_;
    std::shared_ptr<DecisionPolicy> policy_;
    std::vector<Goal> goals_;
    std::vector<std::string> reported_goals_;
    std::shared_ptr<AgentAction> action_;
    std::vector<Observation> perceived_;
    AgentStats stats_;
    double decision_timer_ = 0.0;
    bool has_last_position_ = false;
    double last_x_ = 0.0;
    double last_y_ = 0.0;
};

struct AgentDetectedEvent {
    std::uint64_t agent;
    Observation observation;
};

struct GoalCompletedEvent {
    std::uint64_t agent;
    std::string goal;
};

struct AgentActionEvent {
    std::uint64_t agent;
    std::string action;
    enum class Kind { Started, Succeeded, Failed, Rejected, Interrupted } kind;
};

// Runs perceive -> remember -> evaluate goals -> decide -> act for every active Agent.
class AgentSystem final : public UpdateSystem {
public:
    using ActionValidator = std::function<bool(const ActionContext&, const AgentAction&)>;

    explicit AgentSystem(EventBus* events = nullptr, const NavigationGrid* grid = nullptr);
    ~AgentSystem() override;
    AgentSystem(const AgentSystem&) = delete;
    AgentSystem& operator=(const AgentSystem&) = delete;

    void update(Scene& scene, const InputState& input, double delta_seconds) override;
    // Sounds are normally received from the EventBus; this allows feeding them directly.
    void hear(const SoundProducedEvent& sound) { sounds_.push_back(sound); }
    // Application-level world rules applied to every action in addition to the action's own validation.
    void set_action_validator(ActionValidator validator) { validator_ = std::move(validator); }
    double time() const { return time_; }

private:
    void publish_action(std::uint64_t agent, const AgentAction& action, AgentActionEvent::Kind kind);

    EventBus* events_;
    const NavigationGrid* grid_;
    Subscription sound_subscription_;
    ActionValidator validator_;
    std::vector<SoundProducedEvent> sounds_;
    double time_ = 0.0;
};

} // namespace a2e
