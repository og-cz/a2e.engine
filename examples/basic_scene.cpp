#include "a2e/a2e.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace {

class PlayerControlSystem final : public a2e::UpdateSystem {
public:
    PlayerControlSystem() {
        actions_.bind("move_left", a2e::Key::A);
        actions_.bind("move_left", a2e::Key::Left);
        actions_.bind("move_right", a2e::Key::D);
        actions_.bind("move_right", a2e::Key::Right);
        actions_.bind("move_up", a2e::Key::W);
        actions_.bind("move_up", a2e::Key::Up);
        actions_.bind("move_down", a2e::Key::S);
        actions_.bind("move_down", a2e::Key::Down);
    }

    void update(a2e::Scene& scene, const a2e::InputState& input, double delta_seconds) override {
        auto* player = scene.find_entity(player_id_);
        if (!player) return;
        const double speed = 180.0;
        if (actions_.is_action_down(input, "move_left")) player->transform().x -= speed * delta_seconds;
        if (actions_.is_action_down(input, "move_right")) player->transform().x += speed * delta_seconds;
        if (actions_.is_action_down(input, "move_up")) player->transform().y -= speed * delta_seconds;
        if (actions_.is_action_down(input, "move_down")) player->transform().y += speed * delta_seconds;
    }

    void set_player_id(std::uint64_t id) { player_id_ = id; }

private:
    a2e::InputMap actions_;
    std::uint64_t player_id_ = 0;
};

class CameraControlSystem final : public a2e::UpdateSystem {
public:
    explicit CameraControlSystem(a2e::Camera& camera) : camera_(camera) {
        actions_.bind("zoom_in", a2e::Key::ZoomIn);
        actions_.bind("zoom_out", a2e::Key::ZoomOut);
    }

    void update(a2e::Scene&, const a2e::InputState& input, double delta_seconds) override {
        if (actions_.is_action_down(input, "zoom_in")) camera_.zoom += delta_seconds;
        if (actions_.is_action_down(input, "zoom_out")) camera_.zoom -= delta_seconds;
        camera_.zoom += static_cast<double>(input.mouse_wheel_delta()) / 1200.0;
        if (camera_.zoom < 0.25) camera_.zoom = 0.25;
        if (camera_.zoom > 3.0) camera_.zoom = 3.0;
    }

private:
    a2e::Camera& camera_;
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

} // namespace

int main() {
    a2e::EngineConfig config;
    config.title = "A2E World Demo";

    a2e::Scene scene("Demo Scene");
    auto& tilemap = scene.create_tilemap(20, 12, 40.0, 0x00232C35);
    for (int y = 0; y < tilemap.height(); ++y) {
        for (int x = 0; x < tilemap.width(); ++x) {
            const bool alternating = (x + y) % 2 == 0;
            tilemap.set_tile(x, y, alternating ? 0x0030444A : 0x002B3B42);
        }
    }

    auto& player = scene.create_entity("Demo Entity");
    player.transform().x = 400.0;
    player.transform().y = 240.0;
    player.set_renderable({56.0, 56.0, 0x00F2B84B, 2});
    player.set_collider({56.0, 56.0, 1, 2, false});
    player.set_rigid_body({0.0, 0.0, true});

    auto& marker = scene.create_entity("World Marker");
    marker.transform().x = 220.0;
    marker.transform().y = 140.0;
    marker.transform().rotation = 0.35;
    marker.set_renderable({32.0, 32.0, 0x0048D1CC, 1});

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

    a2e::Application application(config, std::move(scene), std::make_unique<a2e::Basic2DRenderer>());
    application.events().subscribe<a2e::CollisionEnterEvent>([](const a2e::CollisionEnterEvent& event) {
        a2e::log_info("collision entered between entities " + std::to_string(event.first_entity) +
                      " and " + std::to_string(event.second_entity));
    });
    auto controls = std::make_unique<PlayerControlSystem>();
    controls->set_player_id(player.id());
    application.add_system(std::move(controls));
    application.add_system(std::make_unique<CameraControlSystem>(application.camera()));
    application.add_system(std::make_unique<MouseMarkerSystem>(application.camera(), marker.id(),
                                                                config.width, config.height));
    application.add_system(std::make_unique<a2e::PhysicsSystem>(a2e::PhysicsSystem::CollisionCallback{}, 0.0,
                                                                  &application.events(), 0.05));
    return application.run();
}
