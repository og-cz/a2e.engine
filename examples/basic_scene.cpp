#include "a2e/a2e.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace {

class CameraControlSystem final : public a2e::UpdateSystem {
public:
    CameraControlSystem(a2e::Camera& camera, std::uint64_t target_id)
        : camera_(camera), target_id_(target_id) {
        actions_.bind("zoom_in", a2e::Key::ZoomIn);
        actions_.bind("zoom_out", a2e::Key::ZoomOut);
    }

    void update(a2e::Scene& scene, const a2e::InputState& input, double delta_seconds) override {
        const auto* target = scene.find_entity(target_id_);
        if (target != nullptr) {
            camera_.x = target->transform().x;
            camera_.y = target->transform().y;
        }
        if (actions_.is_action_down(input, "zoom_in")) camera_.zoom += delta_seconds;
        if (actions_.is_action_down(input, "zoom_out")) camera_.zoom -= delta_seconds;
        camera_.zoom += static_cast<double>(input.mouse_wheel_delta()) / 1200.0;
        if (camera_.zoom < 0.25) camera_.zoom = 0.25;
        if (camera_.zoom > 3.0) camera_.zoom = 3.0;
    }

private:
    a2e::Camera& camera_;
    std::uint64_t target_id_;
    a2e::InputMap actions_;
};

class MouseMarkerSystem final : public a2e::UpdateSystem {
public:
        MouseMarkerSystem(a2e::Camera& camera, std::uint64_t marker_id,
                                            double viewport_width, double viewport_height)
    : camera_(camera), marker_id_(marker_id),
                    viewport_width_(viewport_width), viewport_height_(viewport_height) {
                actions_.bind_mouse("place_marker", a2e::MouseButton::Left);
        }

    void update(a2e::Scene& scene, const a2e::InputState& input, double) override {
        if (!actions_.is_action_down(input, "place_marker")) return;
        auto* marker = scene.find_entity(marker_id_);
        if (!marker) return;
        const auto world = camera_.screen_to_world(input.mouse_x(), input.mouse_y(),
                               viewport_width_, viewport_height_);
        marker->transform().x = world.first;
        marker->transform().y = world.second;
    }

private:
    a2e::Camera& camera_;
    std::uint64_t marker_id_;
    double viewport_width_;
    double viewport_height_;
    a2e::InputMap actions_;
};

// Marks the player as a moving obstacle so navigating NPCs route around it.
class PlayerObstacleSystem final : public a2e::UpdateSystem {
public:
    PlayerObstacleSystem(a2e::NavigationGrid& grid, std::uint64_t player_id)
        : grid_(grid), player_id_(player_id) {}

    void update(a2e::Scene& scene, const a2e::InputState&, double) override {
        grid_.clear_obstacles();
        const auto* player = scene.find_entity(player_id_);
        if (!player || !player->collider()) return;
        const auto& transform = player->transform();
        const double half_width = player->collider()->width / 2.0;
        const double half_height = player->collider()->height / 2.0;
        grid_.add_obstacle_rect(transform.x - half_width, transform.y - half_height,
                                transform.x + half_width, transform.y + half_height);
    }

private:
    a2e::NavigationGrid& grid_;
    std::uint64_t player_id_;
};

// Example gameplay rule: the NPC walks to the marker. Navigation decides how it gets there.
class FollowMarkerSystem final : public a2e::UpdateSystem {
public:
    FollowMarkerSystem(std::uint64_t npc_id, std::uint64_t marker_id) : npc_id_(npc_id), marker_id_(marker_id) {}

    void update(a2e::Scene& scene, const a2e::InputState&, double delta_seconds) override {
        auto* npc = scene.find_entity(npc_id_);
        const auto* marker = scene.find_entity(marker_id_);
        if (!npc || !marker) return;
        auto* agent = npc->get_component<a2e::NavigationAgent>();
        if (!agent) return;
        const double x = marker->transform().x;
        const double y = marker->transform().y;
        retry_timer_ -= delta_seconds;
        const bool moved = x != target_x_ || y != target_y_;
        const bool retry = agent->status == a2e::NavigationStatus::Unreachable && retry_timer_ <= 0.0;
        if (moved || retry) {
            agent->set_destination(x, y);
            target_x_ = x;
            target_y_ = y;
            retry_timer_ = 0.5;
        }
    }

private:
    std::uint64_t npc_id_;
    std::uint64_t marker_id_;
    double target_x_ = 0.0;
    double target_y_ = 0.0;
    double retry_timer_ = 0.0;
};

} // namespace

int main() {
    a2e::EngineConfig config;
    config.title = "A2E World Demo";
    config.fixed_update_hz = 60.0;

    a2e::Scene scene("Demo Scene");
    auto& tilemap = scene.create_tilemap(20, 12, 40.0, 0x00232C35);
    for (int y = 0; y < tilemap.height(); ++y) {
        for (int x = 0; x < tilemap.width(); ++x) {
            const bool alternating = (x + y) % 2 == 0;
            tilemap.set_tile(x, y, alternating ? 0x0030444A : 0x002B3B42);
        }
    }
    // Interior walls: drawn by the tilemap, solid for physics, and unwalkable for navigation.
    const std::uint32_t wall_color = 0x00596A73;
    for (int y = 1; y <= 8; ++y) tilemap.set_tile(13, y, wall_color);
    for (int x = 3; x <= 9; ++x) tilemap.set_tile(x, 9, wall_color);
    auto& column_wall = scene.create_entity("Column Wall");
    column_wall.transform().x = 540.0;
    column_wall.transform().y = 200.0;
    column_wall.set_collider({40.0, 320.0, 2, 1, false});
    auto& row_wall = scene.create_entity("Row Wall");
    row_wall.transform().x = 260.0;
    row_wall.transform().y = 380.0;
    row_wall.set_collider({280.0, 40.0, 2, 1, false});
    // Declared before the application so it outlives the navigation systems that reference it.
    auto navigation_grid = a2e::NavigationGrid::from_tilemap(
        tilemap, [wall_color](std::uint32_t tile) { return tile != wall_color; });

    auto& player = scene.create_entity("Demo Entity");
    player.transform().x = 400.0;
    player.transform().y = 240.0;
    player.set_renderable({56.0, 56.0, 0x00F2B84B, 2});
    player.set_collider({56.0, 56.0, 1, 2, false});
    player.set_rigid_body({0.0, 0.0, true});

    auto& marker = scene.create_entity("World Marker");
    marker.transform().x = 220.0;
    marker.transform().y = 140.0;
    marker.transform().rotation = 0.0;
    marker.set_renderable({32.0, 32.0, 0x0048D1CC, 1});
    // Two 2x2 frames side by side: a checker and its inverse.
    auto marker_texture = std::make_shared<a2e::Texture>(4, 2, std::vector<std::uint32_t>{
        0x00FFFFFF, 0x0048D1CC, 0x0048D1CC, 0x00FFFFFF,
        0x0048D1CC, 0x00FFFFFF, 0x00FFFFFF, 0x0048D1CC});
    marker.set_sprite({marker_texture, 0, 0, 2, 2});
    marker.add_component<a2e::Animator>().add_state("pulse", a2e::AnimationClip::from_grid(2, 2, 2, 0, 2, 0.25));

    auto& npc = scene.create_entity("Navigating NPC");
    npc.transform().x = 700.0;
    npc.transform().y = 60.0;
    npc.set_renderable({28.0, 28.0, 0x00E0607E, 2});
    npc.add_component<a2e::NavigationAgent>().speed = 110.0;

    auto& left_wall = scene.create_entity("Left Boundary");
    left_wall.transform().x = -24.0;
    left_wall.transform().y = 240.0;
    left_wall.set_collider({48.0, 520.0, 2, 1, false});

    auto& right_wall = scene.create_entity("Right Boundary");
    right_wall.transform().x = 824.0;
    right_wall.transform().y = 240.0;
    right_wall.set_collider({48.0, 520.0, 2, 1, false});

    auto& top_wall = scene.create_entity("Top Boundary");
    top_wall.transform().x = 400.0;
    top_wall.transform().y = -24.0;
    top_wall.set_collider({848.0, 48.0, 2, 1, false});

    auto& bottom_wall = scene.create_entity("Bottom Boundary");
    bottom_wall.transform().x = 400.0;
    bottom_wall.transform().y = 504.0;
    bottom_wall.set_collider({848.0, 48.0, 2, 1, false});

    // Declared before the application so it outlives the systems that reference it.
    a2e::AudioManager audio(a2e::create_audio_backend());
    audio.set_sound_volume(0.4);
    auto bump_sound = std::make_shared<a2e::AudioClip>(a2e::AudioClip::tone(220.0, 0.08));

    a2e::Application application(config, std::move(scene), std::make_unique<a2e::Basic2DRenderer>());
    application.resources().register_resource("marker_texture", marker_texture);
    application.resources().register_resource("bump_sound", bump_sound);
    application.events().subscribe<a2e::CollisionEnterEvent>([&audio, bump_sound](const a2e::CollisionEnterEvent& event) {
        a2e::log_info("collision entered between entities " + std::to_string(event.first_entity) +
                      " and " + std::to_string(event.second_entity));
        if (!event.trigger) audio.play_sound(bump_sound);
    });
    application.add_system(std::make_unique<a2e::PlayerController>(player.id(), 180.0));
    application.add_system(std::make_unique<CameraControlSystem>(application.camera(), player.id()));
    application.add_system(std::make_unique<MouseMarkerSystem>(application.camera(), marker.id(),
                                                                config.width, config.height));
    application.add_system(std::make_unique<PlayerObstacleSystem>(navigation_grid, player.id()));
    application.add_system(std::make_unique<FollowMarkerSystem>(npc.id(), marker.id()));
    application.add_system(std::make_unique<a2e::NavigationSystem>(navigation_grid, &application.events()));
    application.add_system(std::make_unique<a2e::AnimationSystem>(&application.events()));
    application.add_system(std::make_unique<a2e::AudioSystem>(audio));
    application.add_fixed_system(std::make_unique<a2e::PhysicsSystem>(a2e::PhysicsSystem::CollisionCallback{}, 0.0,
                                                                       &application.events(), 0.05));
    return application.run();
}
