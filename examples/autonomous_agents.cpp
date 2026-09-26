// A guard built only from A2E's generic agent pieces: sensors, memory, goals, a rule policy,
// and validated actions. Everything guard-specific lives here, not in the engine.
#include "a2e/a2e.hpp"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr double tile_size = 40.0;
constexpr std::uint32_t wall_color = 0x00596A73;
constexpr double player_start_x = 180.0;
constexpr double player_start_y = 220.0;

std::pair<double, double> cell_center(int x, int y) { return {(x + 0.5) * tile_size, (y + 0.5) * tile_size}; }

// The newest observation of the player this update, if the guard can currently see them.
const a2e::Observation* sighting(const a2e::Agent& agent) {
    for (const auto& observation : agent.perceived()) {
        if (observation.kind == "seen" && observation.tag == "player") return &observation;
    }
    return nullptr;
}

class NoiseSystem final : public a2e::UpdateSystem {
public:
    NoiseSystem(a2e::EventBus& events, std::uint64_t player_id) : events_(events), player_id_(player_id) {
        actions_.bind("make_noise", a2e::Key::Space);
    }

    void update(a2e::Scene& scene, const a2e::InputState& input, double) override {
        if (!actions_.is_action_pressed(input, "make_noise")) return;
        const auto* player = scene.find_entity(player_id_);
        if (!player) return;
        events_.publish(a2e::SoundProducedEvent{player_id_, player->transform().x, player->transform().y, 260.0, "player"});
        a2e::log_info("player made a noise");
    }

private:
    a2e::EventBus& events_;
    std::uint64_t player_id_;
    a2e::InputMap actions_;
};

// Colors the guard by its top goal so decisions are visible without the debug overlay.
class GuardAppearanceSystem final : public a2e::UpdateSystem {
public:
    explicit GuardAppearanceSystem(std::uint64_t guard_id) : guard_id_(guard_id) {}

    void update(a2e::Scene& scene, const a2e::InputState&, double) override {
        auto* guard = scene.find_entity(guard_id_);
        if (!guard || !guard->renderable()) return;
        const auto* goal = guard->get_component<a2e::Agent>()->top_goal();
        const std::string name = goal ? goal->name : "";
        guard->renderable()->color = name == "chase" ? 0x00E5484D : name == "investigate" ? 0x00F59E3B : 0x004C7CF0;
    }

private:
    std::uint64_t guard_id_;
};

class CatchSystem final : public a2e::UpdateSystem {
public:
    CatchSystem(std::uint64_t guard_id, std::uint64_t player_id) : guard_id_(guard_id), player_id_(player_id) {}

    void update(a2e::Scene& scene, const a2e::InputState&, double) override {
        auto* guard = scene.find_entity(guard_id_);
        auto* player = scene.find_entity(player_id_);
        if (!guard || !player) return;
        const double dx = guard->transform().x - player->transform().x;
        const double dy = guard->transform().y - player->transform().y;
        if (dx * dx + dy * dy > 34.0 * 34.0) return;
        a2e::log_info("caught! resetting the player");
        player->transform().x = player_start_x;
        player->transform().y = player_start_y;
        guard->get_component<a2e::Agent>()->memory.clear();
    }

private:
    std::uint64_t guard_id_;
    std::uint64_t player_id_;
};

class DebugToggleSystem final : public a2e::UpdateSystem {
public:
    explicit DebugToggleSystem(a2e::DebugRenderer& debug) : debug_(debug) { actions_.bind("toggle_debug", a2e::Key::F1); }

    void update(a2e::Scene&, const a2e::InputState& input, double) override {
        if (actions_.is_action_pressed(input, "toggle_debug")) debug_.toggle();
    }

private:
    a2e::DebugRenderer& debug_;
    a2e::InputMap actions_;
};

std::shared_ptr<a2e::RulePolicy> make_guard_policy() {
    auto patrol_index = std::make_shared<std::size_t>(0);
    const std::vector<std::pair<double, double>> patrol = {cell_center(2, 2),  cell_center(2, 9),
                                                           cell_center(10, 9), cell_center(10, 2),
                                                           cell_center(17, 2), cell_center(17, 9)};
    const auto top_goal_is = [](const char* name) {
        return [name](const a2e::DecisionContext& context) {
            const auto* goal = context.agent.top_goal();
            return goal && goal->name == name;
        };
    };

    auto policy = std::make_shared<a2e::RulePolicy>();
    policy->add_rule("chase", top_goal_is("chase"), [](const a2e::DecisionContext& context) {
        const auto* seen = sighting(context.agent);
        return seen ? std::make_shared<a2e::MoveToAction>(seen->x, seen->y, 8.0) : nullptr;
    });
    policy->add_rule("look_around", top_goal_is("investigate"), [](const a2e::DecisionContext& context) {
        const auto* memory = context.agent.memory.latest("player");
        if (!memory) return std::shared_ptr<a2e::AgentAction>{};
        const double dx = memory->observation.x - context.self.transform().x;
        const double dy = memory->observation.y - context.self.transform().y;
        if (dx * dx + dy * dy > 24.0 * 24.0) return std::shared_ptr<a2e::AgentAction>{};
        return std::shared_ptr<a2e::AgentAction>(std::make_shared<a2e::WaitAction>(1.5));
    });
    policy->add_rule("investigate", top_goal_is("investigate"), [](const a2e::DecisionContext& context) {
        const auto* memory = context.agent.memory.latest("player");
        return memory ? std::make_shared<a2e::MoveToAction>(memory->observation.x, memory->observation.y) : nullptr;
    });
    policy->add_rule("patrol", nullptr, [patrol, patrol_index](const a2e::DecisionContext& context) {
        auto target = patrol[*patrol_index];
        const double dx = target.first - context.self.transform().x;
        const double dy = target.second - context.self.transform().y;
        if (dx * dx + dy * dy < 20.0 * 20.0) {
            *patrol_index = (*patrol_index + 1) % patrol.size();
            target = patrol[*patrol_index];
        }
        return std::make_shared<a2e::MoveToAction>(target.first, target.second);
    });
    return policy;
}

} // namespace

int main() {
    a2e::EngineConfig config;
    config.title = "A2E Autonomous Agents";
    config.width = 800;
    config.height = 480;
    config.fixed_update_hz = 60.0;

    a2e::Scene scene("Guard Scene");
    auto& tilemap = scene.create_tilemap(20, 12, tile_size, 0x00232C35);
    for (int y = 0; y < tilemap.height(); ++y) {
        for (int x = 0; x < tilemap.width(); ++x) {
            const bool border = x == 0 || y == 0 || x == tilemap.width() - 1 || y == tilemap.height() - 1;
            const bool inner = (x == 7 && y <= 7) || (x == 13 && y >= 4);
            tilemap.set_tile(x, y, border || inner ? wall_color : ((x + y) % 2 == 0 ? 0x0030444A : 0x002B3B42));
        }
    }
    // Each wall tile is also a static collider so the player cannot walk through it.
    for (int y = 0; y < tilemap.height(); ++y) {
        for (int x = 0; x < tilemap.width(); ++x) {
            if (tilemap.tile(x, y) != wall_color) continue;
            auto& wall = scene.create_entity("Wall");
            const auto [center_x, center_y] = cell_center(x, y);
            wall.transform().x = center_x;
            wall.transform().y = center_y;
            wall.set_collider({tile_size, tile_size, 2, 1, false});
        }
    }
    // Walls block movement and sight. Declared before the application so it outlives the systems using it.
    const auto grid = a2e::NavigationGrid::from_tilemap(tilemap, [](std::uint32_t tile) { return tile != wall_color; });

    auto& player = scene.create_entity("Player");
    player.transform().x = player_start_x;
    player.transform().y = player_start_y;
    player.set_renderable({28.0, 28.0, 0x00F2B84B, 2});
    player.set_collider({28.0, 28.0, 1, 2, false});
    player.set_rigid_body({0.0, 0.0, true});
    player.add_component<a2e::Perceivable>().tag = "player";

    auto& guard = scene.create_entity("Guard");
    const auto [guard_x, guard_y] = cell_center(17, 9);
    guard.transform().x = guard_x;
    guard.transform().y = guard_y;
    guard.set_renderable({30.0, 30.0, 0x004C7CF0, 2});
    guard.add_component<a2e::NavigationAgent>().speed = 95.0;
    auto& agent = guard.add_component<a2e::Agent>();
    agent.memory = a2e::AgentMemory(0.2);
    agent.add_sensor(std::make_shared<a2e::VisionSensor>(230.0, 100.0 * 3.141592653589793 / 180.0));
    agent.add_sensor(std::make_shared<a2e::HearingSensor>());
    agent.add_goal({"patrol", 0.2, false, nullptr});
    agent.add_goal({"investigate", 0.0, false, [](const a2e::Agent& self, double) {
                        const auto* memory = self.memory.latest("player");
                        return memory ? memory->strength * 0.8 : 0.0;
                    }});
    agent.add_goal({"chase", 0.0, false, [](const a2e::Agent& self, double) { return sighting(self) ? 1.0 : 0.0; }});
    agent.set_policy(make_guard_policy());

    auto debug = std::make_unique<a2e::DebugRenderer>(std::make_unique<a2e::Basic2DRenderer>(), &grid);
    auto& debug_view = *debug;
    a2e::Application application(config, std::move(scene), std::move(debug));
    application.events().subscribe<a2e::AgentDetectedEvent>([](const a2e::AgentDetectedEvent& event) {
        a2e::log_info("guard noticed the " + event.observation.tag + " (" + event.observation.kind + ")");
    });

    application.add_system(std::make_unique<a2e::PlayerController>(player.id(), 160.0));
    application.add_system(std::make_unique<NoiseSystem>(application.events(), player.id()));
    application.add_system(std::make_unique<a2e::AgentSystem>(&application.events(), &grid));
    application.add_system(std::make_unique<a2e::NavigationSystem>(grid, &application.events()));
    application.add_system(std::make_unique<GuardAppearanceSystem>(guard.id()));
    application.add_system(std::make_unique<CatchSystem>(guard.id(), player.id()));
    application.add_system(std::make_unique<DebugToggleSystem>(debug_view));
    application.add_fixed_system(std::make_unique<a2e::PhysicsSystem>(a2e::PhysicsSystem::CollisionCallback{}, 0.0,
                                                                       nullptr, 0.05));
    return application.run();
}
