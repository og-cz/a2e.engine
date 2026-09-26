// Tests rely on assert, so keep it active even in Release builds.
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "a2e/agent.hpp"
#include "a2e/navigation.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double pi = 3.141592653589793;

std::vector<a2e::Observation> sense(const a2e::Sensor& sensor, const a2e::Scene& scene, const a2e::Entity& self,
                                    const std::vector<a2e::SoundProducedEvent>& sounds = {},
                                    const a2e::NavigationGrid* grid = nullptr) {
    std::vector<a2e::Observation> observations;
    sensor.sense({scene, self, 1.0, 0.0, 2.0, sounds, grid}, observations);
    return observations;
}

void perception_works() {
    a2e::Scene scene("Perception Scene");
    auto& watcher = scene.create_entity("Watcher");
    watcher.transform().x = 5.0;
    watcher.transform().y = 5.0;
    auto& ahead = scene.create_entity("Ahead");
    ahead.transform().x = 45.0;
    ahead.transform().y = 5.0;
    ahead.add_component<a2e::Perceivable>().tag = "player";
    auto& behind = scene.create_entity("Behind");
    behind.transform().x = -15.0;
    behind.transform().y = 5.0;
    behind.add_component<a2e::Perceivable>().tag = "crate";
    auto& unmarked = scene.create_entity("Unmarked");
    unmarked.transform().x = 25.0;
    unmarked.transform().y = 5.0;

    const a2e::VisionSensor vision(80.0, pi / 2.0);
    auto seen = sense(vision, scene, watcher);
    assert(seen.size() == 1);
    assert(seen[0].source == ahead.id() && seen[0].kind == "seen" && seen[0].tag == "player");
    assert(seen[0].time == 2.0);
    assert(std::abs(seen[0].confidence - 0.75) < 1e-9);

    assert(sense(a2e::VisionSensor(30.0, pi / 2.0), scene, watcher).empty());
    assert(sense(a2e::VisionSensor(80.0, 2.0 * pi), scene, watcher).size() == 2);
    ahead.get_component<a2e::Perceivable>()->detectable = false;
    assert(sense(vision, scene, watcher).empty());
    ahead.get_component<a2e::Perceivable>()->detectable = true;

    a2e::NavigationGrid grid(6, 1, 10.0);
    grid.set_walkable({2, 0}, false);
    assert(sense(vision, scene, watcher, {}, &grid).empty());
    assert(!a2e::has_line_of_sight(grid, 5.0, 5.0, 45.0, 5.0));
    assert(a2e::has_line_of_sight(grid, 5.0, 5.0, 15.0, 5.0));
    grid.add_obstacle({2, 0});
    grid.set_walkable({2, 0}, true);
    assert(sense(vision, scene, watcher, {}, &grid).size() == 1);

    const std::vector<a2e::SoundProducedEvent> sounds = {
        {ahead.id(), 45.0, 5.0, 50.0, "footstep"},
        {behind.id(), 205.0, 5.0, 50.0, "far"},
        {watcher.id(), 5.0, 5.0, 50.0, "self"}};
    const auto heard = sense(a2e::HearingSensor(), scene, watcher, sounds);
    assert(heard.size() == 1);
    assert(heard[0].kind == "heard" && heard[0].tag == "footstep");
    assert(heard[0].confidence > 0.4 && heard[0].confidence < 0.5);
    assert(sense(a2e::HearingSensor(0.5), scene, watcher, sounds).empty());

    const auto nearby = sense(a2e::ProximitySensor(25.0), scene, watcher);
    assert(nearby.size() == 1 && nearby[0].source == behind.id());

    bool rejected = false;
    try { a2e::VisionSensor invalid(10.0, 0.0); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

void memory_works() {
    a2e::AgentMemory memory(0.5, 0.1, 2, 0.8);
    memory.remember({7, "seen", "player", 1.0, 2.0, 1.0, 0.9});
    memory.remember({7, "seen", "player", 3.0, 4.0, 2.0, 0.6});
    assert(memory.size() == 1);
    assert(memory.records()[0].observation.x == 3.0);
    assert(memory.records()[0].strength == 0.9);
    memory.remember({7, "heard", "player", 5.0, 6.0, 3.0, 0.5});
    assert(memory.size() == 2);
    assert(memory.latest("player")->observation.kind == "heard");

    memory.remember({9, "seen", "treasure", 0.0, 0.0, 4.0, 1.0}, 0.9);
    assert(memory.size() == 3);
    memory.remember({10, "seen", "rock", 0.0, 0.0, 5.0, 0.2});
    assert(memory.size() == 3);
    assert(memory.recall([](const a2e::MemoryRecord& record) { return record.observation.tag == "rock"; }).empty());

    memory.update(1.0);
    assert(memory.size() == 2);
    const auto* treasure = memory.latest("treasure");
    assert(treasure && treasure->long_term && std::abs(treasure->strength - 0.95) < 1e-9);
    assert(std::abs(memory.latest("player")->strength - 0.4) < 1e-9);
    assert(memory.forget(9, "seen"));
    assert(!memory.forget(9, "seen"));

    a2e::Beliefs beliefs;
    beliefs.set("door_open", true, 0.7, 1.5);
    beliefs.set("target", std::make_pair(10.0, 20.0));
    beliefs.set("mood", "curious");
    beliefs.set("threat", 0.4);
    assert(beliefs.value<bool>("door_open") == true);
    assert(beliefs.get("door_open")->confidence == 0.7);
    assert((beliefs.value<std::pair<double, double>>("target") == std::make_pair(10.0, 20.0)));
    assert(beliefs.value<std::string>("mood") == std::string("curious"));
    assert(!beliefs.value<bool>("mood"));
    assert(beliefs.value<double>("threat") == 0.4);
    assert(beliefs.forget("threat") && beliefs.size() == 3);
}

class ScriptedPolicy final : public a2e::DecisionPolicy {
public:
    std::shared_ptr<a2e::AgentAction> decide(const a2e::DecisionContext&) override { return next; }
    std::shared_ptr<a2e::AgentAction> next;
};

void goals_and_rules_work() {
    a2e::Agent agent;
    agent.add_goal({"patrol", 0.3});
    agent.add_goal({"flee", 0.0, false, [](const a2e::Agent&, double time) { return time > 5.0 ? 0.9 : 0.1; }});
    agent.update_goals(1.0);
    assert(agent.top_goal()->name == "patrol");
    agent.update_goals(6.0);
    assert(agent.top_goal()->name == "flee");
    agent.goal("flee")->completed = true;
    assert(agent.top_goal()->name == "patrol");
    agent.add_goal({"patrol", 0.05});
    assert(agent.goals().size() == 2 && agent.goal("patrol")->priority == 0.05);

    a2e::Scene scene("Rules");
    auto& self = scene.create_entity("Self");
    a2e::RulePolicy rules;
    bool alarm = false;
    rules.add_rule("alarm", [&alarm](const a2e::DecisionContext&) { return alarm; },
                   [](const a2e::DecisionContext&) { return std::make_shared<a2e::WaitAction>(1.0); })
        .add_rule("skip", nullptr, [](const a2e::DecisionContext&) { return nullptr; })
        .add_rule("idle", nullptr, [](const a2e::DecisionContext&) { return std::make_shared<a2e::WaitAction>(2.0); });
    const a2e::DecisionContext context{scene, self, agent, 0.0};
    assert(rules.decide(context) && rules.last_rule() == "idle");
    alarm = true;
    assert(rules.decide(context) && rules.last_rule() == "alarm");
}

void agent_loop_works() {
    a2e::NavigationGrid grid(8, 3, 10.0);
    grid.allow_diagonal = false;
    grid.set_walkable({4, 0}, false);
    grid.set_walkable({4, 1}, false);

    a2e::Scene scene("Agent Scene");
    auto& guard = scene.create_entity("Guard");
    guard.transform().x = 15.0;
    guard.transform().y = 5.0;
    guard.add_component<a2e::NavigationAgent>().speed = 40.0;
    auto& agent = guard.add_component<a2e::Agent>();
    agent.add_sensor(std::make_shared<a2e::VisionSensor>(100.0, pi));
    agent.add_sensor(std::make_shared<a2e::HearingSensor>());
    auto policy = std::make_shared<ScriptedPolicy>();
    agent.set_policy(policy);

    auto& intruder = scene.create_entity("Intruder");
    intruder.transform().x = 35.0;
    intruder.transform().y = 5.0;
    intruder.add_component<a2e::Perceivable>().tag = "intruder";

    a2e::EventBus events;
    int detections = 0;
    std::vector<a2e::AgentActionEvent::Kind> actions;
    std::string completed_goal;
    events.subscribe<a2e::AgentDetectedEvent>([&detections](const a2e::AgentDetectedEvent&) { ++detections; });
    events.subscribe<a2e::AgentActionEvent>([&actions](const a2e::AgentActionEvent& event) {
        actions.push_back(event.kind);
    });
    events.subscribe<a2e::GoalCompletedEvent>([&completed_goal](const a2e::GoalCompletedEvent& event) {
        completed_goal = event.goal;
    });
    a2e::AgentSystem agents(&events, &grid);
    a2e::NavigationSystem navigation(grid, &events);
    const auto step = [&](double seconds) {
        agents.update(scene, a2e::InputState{}, seconds);
        navigation.update(scene, a2e::InputState{}, seconds);
    };

    step(0.1);
    assert(detections == 1);
    assert(agent.perceived().size() == 1);
    assert(agent.memory.latest("intruder") != nullptr);
    step(0.1);
    assert(detections == 1);

    // Blocked target: the action's own validation rejects it.
    policy->next = std::make_shared<a2e::MoveToAction>(45.0, 5.0);
    step(0.3);
    assert(actions.back() == a2e::AgentActionEvent::Kind::Rejected);
    assert(agent.current_action() == nullptr);
    assert(agent.stats().actions_rejected == 1);

    // Application rules can veto otherwise valid actions.
    agents.set_action_validator([](const a2e::ActionContext&, const a2e::AgentAction& action) {
        return action.name() != "wait";
    });
    policy->next = std::make_shared<a2e::WaitAction>(1.0);
    step(0.3);
    assert(agent.stats().actions_rejected == 2);
    agents.set_action_validator(nullptr);

    // A valid move walks around the wall using navigation.
    policy->next = std::make_shared<a2e::MoveToAction>(65.0, 5.0, 2.0);
    step(0.3);
    assert(actions.back() == a2e::AgentActionEvent::Kind::Started);
    assert(agent.current_action() && agent.current_action()->name() == "move_to");
    for (int frame = 0; frame < 200 && agent.current_action(); ++frame) step(0.05);
    assert(agent.current_action() == nullptr);
    assert(actions.back() == a2e::AgentActionEvent::Kind::Succeeded);
    assert(std::abs(guard.transform().x - 65.0) <= 2.0);
    assert(agent.stats().actions_succeeded == 1);
    assert(agent.facing_y != 0.0 || agent.facing_x > 0.0);

    // Proposing an equivalent action does not restart it; a different one interrupts.
    policy->next = std::make_shared<a2e::WaitAction>(10.0);
    step(0.3);
    const auto* waiting = agent.current_action();
    step(0.3);
    assert(agent.current_action() == waiting);
    policy->next = std::make_shared<a2e::MoveToAction>(55.0, 25.0);
    step(0.3);
    assert(actions[actions.size() - 2] == a2e::AgentActionEvent::Kind::Interrupted);
    policy->next = nullptr;

    // Sounds published on the bus are heard once, then cleared.
    intruder.set_active(false);
    events.publish(a2e::SoundProducedEvent{intruder.id(), 60.0, 20.0, 30.0, "noise"});
    step(0.1);
    assert(agent.perceived().size() == 1 && agent.perceived()[0].kind == "heard");
    step(0.1);
    assert(agent.perceived().empty());

    agent.add_goal({"investigate", 0.5}).completed = true;
    step(0.1);
    assert(completed_goal == "investigate");
    assert(agents.time() > 0.0);
    assert(agent.stats().decisions > 0);
}

} // namespace

int main() {
    perception_works();
    memory_works();
    goals_and_rules_work();
    agent_loop_works();
    std::cout << "AI tests passed\n";
}
