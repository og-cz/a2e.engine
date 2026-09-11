#include "a2e/a2e.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class FakeTarget final : public a2e::RenderTarget {
public:
    std::int32_t width() const override { return 320; }
    std::int32_t height() const override { return 200; }
    void clear(std::uint32_t) override { ++clear_count; }
    void fill_rectangle(double, double, double, double, std::uint32_t) override { ++rectangle_count; }
    void fill_polygon(const std::vector<std::pair<double, double>>&, std::uint32_t) override { ++polygon_count; }
    void present() override { ++present_count; }
    int clear_count = 0;
    int rectangle_count = 0;
    int polygon_count = 0;
    int present_count = 0;
};

class FakeWindow final : public a2e::Window {
public:
    bool process_events() override {
        input_.begin_frame();
        return frame_count++ == 0;
    }
    void close() override { open_ = false; }
    bool is_open() const override { return open_; }
    const a2e::InputState& input() const override { return input_; }
    std::int32_t width() const override { return 320; }
    std::int32_t height() const override { return 200; }
    void clear(std::uint32_t) override {}
    void fill_rectangle(double, double, double, double, std::uint32_t) override {}
    void present() override {}
    void fill_polygon(const std::vector<std::pair<double, double>>&, std::uint32_t) override {}

private:
    a2e::InputState input_;
    int frame_count = 0;
    bool open_ = true;
};

class FakeRenderer final : public a2e::Renderer {
public:
    void render(const a2e::Scene&, a2e::RenderTarget&, const a2e::Camera&) override {}
};

class RecordingSystem final : public a2e::UpdateSystem {
public:
    RecordingSystem(std::vector<std::string>& order, std::string name)
        : order_(order), name_(std::move(name)) {}

    void update(a2e::Scene&, const a2e::InputState&, double) override { order_.push_back(name_); }

private:
    std::vector<std::string>& order_;
    std::string name_;
};

struct TestEvent {
    int value = 0;
};

struct Health {
    int value = 100;
    explicit Health(int initial_value) : value(initial_value) {}
};

void scene_and_renderer_work() {
    a2e::Scene scene("Test Scene");
    auto& entity = scene.create_entity("Box");
    entity.transform().x = 10.0;
    entity.transform().y = 20.0;
    entity.set_renderable({20.0, 10.0, 0x00FFFFFF});
    assert(scene.entities().size() == 1);
    assert(scene.find_entity(entity.id()) == &entity);
    const a2e::Scene& read_only_scene = scene;
    assert(read_only_scene.find_entity(entity.id()) == &entity);

    FakeTarget target;
    a2e::Basic2DRenderer renderer;
    a2e::Camera camera{10.0, 20.0, 1.0};
    renderer.render(scene, target, camera);
    assert(target.rectangle_count == 1);
    entity.transform().rotation = 0.5;
    renderer.render(scene, target, camera);
    assert(target.polygon_count == 1);
    entity.renderable()->visible = false;
    renderer.render(scene, target, camera);
    assert(target.polygon_count == 1);
    entity.renderable()->visible = true;
    entity.set_active(false);
    renderer.render(scene, target, camera);
    assert(target.rectangle_count == 1);
    entity.set_active(true);
}

void resources_and_configuration_work() {
    a2e::EngineConfig config;
    config.validate();
    bool rejected = false;
    try { a2e::EngineConfig invalid{0}; invalid.validate(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    a2e::ResourceManager resources;
    resources.register_resource("number", std::make_shared<int>(42));
    assert(*resources.get<int>("number") == 42);
    assert(resources.size() == 1);

    a2e::Scene component_scene("Components");
    auto& component_entity = component_scene.create_entity("Custom Entity");
    component_entity.add_component<Health>(75);
    assert(component_entity.get_component<Health>()->value == 75);
    component_entity.add_component<Health>(40);
    assert(component_entity.get_component<Health>()->value == 40);
    assert(component_entity.remove_component<Health>());
    assert(component_entity.get_component<Health>() == nullptr);

    a2e::EventBus events;
    int observed = 0;
    events.subscribe<TestEvent>([&observed](const TestEvent& event) { observed = event.value; });
    events.publish(TestEvent{42});
    assert(observed == 42);
    int cancelled_observations = 0;
    auto subscription = events.subscribe<TestEvent>([&cancelled_observations](const TestEvent&) {
        ++cancelled_observations;
    });
    events.publish(TestEvent{43});
    assert(cancelled_observations == 1);
    subscription.unsubscribe();
    events.publish(TestEvent{44});
    assert(cancelled_observations == 1);
}

void input_and_system_pipeline_work() {
    a2e::InputState input;
    input.set_key(a2e::Key::Space, true);
    assert(input.pressed(a2e::Key::Space));
    input.begin_frame();
    assert(input.is_down(a2e::Key::Space));
    assert(!input.pressed(a2e::Key::Space));
    input.set_key(a2e::Key::Space, false);
    assert(input.released(a2e::Key::Space));

    a2e::InputMap actions;
    actions.bind("jump", a2e::Key::Space);
    actions.bind("jump", a2e::Key::Up);
    input.set_key(a2e::Key::Up, true);
    assert(actions.is_action_down(input, "jump"));
    assert(actions.is_action_pressed(input, "jump"));
    actions.clear_action("jump");
    assert(!actions.is_action_down(input, "jump"));
    actions.bind_mouse("jump", a2e::MouseButton::Left);
    input.set_mouse_button(a2e::MouseButton::Left, true);
    assert(actions.is_action_down(input, "jump"));
    actions.clear_action("jump");
    assert(!actions.is_action_down(input, "jump"));

    input.set_mouse_position(120, 80);
    input.set_mouse_button(a2e::MouseButton::Left, true);
    assert(input.mouse_x() == 120);
    assert(input.mouse_y() == 80);
    assert(input.mouse_pressed(a2e::MouseButton::Left));
    input.begin_frame();
    assert(input.is_mouse_down(a2e::MouseButton::Left));
    assert(!input.mouse_pressed(a2e::MouseButton::Left));
    input.set_mouse_button(a2e::MouseButton::Left, false);
    assert(input.mouse_released(a2e::MouseButton::Left));

    a2e::GamepadState gamepad;
    gamepad.set_button(a2e::GamepadButton::South, true);
    gamepad.set_left_stick(0.5, -1.0);
    assert(gamepad.pressed(a2e::GamepadButton::South));
    assert(gamepad.left_stick_x() == 0.5);
    assert(gamepad.left_stick_y() == -1.0);
    gamepad.begin_frame();
    assert(gamepad.is_down(a2e::GamepadButton::South));
    gamepad.set_button(a2e::GamepadButton::South, false);
    assert(gamepad.released(a2e::GamepadButton::South));
    actions.bind_gamepad("jump", a2e::GamepadButton::South);
    gamepad.set_button(a2e::GamepadButton::South, true);
    assert(actions.is_action_down(gamepad, "jump"));
    actions.clear_action("jump");
    assert(!actions.is_action_down(gamepad, "jump"));

    std::vector<std::string> order;
    auto window = std::make_unique<FakeWindow>();
    a2e::Application application(
        a2e::EngineConfig{}, a2e::Scene("Pipeline Scene"),
        std::make_unique<FakeRenderer>(), std::move(window));
    application.add_system(std::make_unique<RecordingSystem>(order, "first"));
    application.add_system(std::make_unique<RecordingSystem>(order, "second"));
    application.run();
    assert((order == std::vector<std::string>{"first", "second"}));
}

void camera_validation_works() {
    a2e::Camera camera;
    camera.validate();
    camera.x = 10.0;
    camera.y = 20.0;
    camera.zoom = 2.0;
    const auto screen = camera.world_to_screen(15.0, 25.0, 320.0, 200.0);
    assert(screen.first == 170.0);
    assert(screen.second == 110.0);
    const auto world = camera.screen_to_world(screen.first, screen.second, 320.0, 200.0);
    assert(world.first == 15.0);
    assert(world.second == 25.0);
    camera.zoom = 0.0;
    bool rejected = false;
    try { camera.validate(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

void tilemap_work() {
    a2e::Scene scene("Tilemap Scene");
    auto& tilemap = scene.create_tilemap(2, 3, 16.0);
    tilemap.set_tile(1, 2, 0x00FFFFFF);
    assert(scene.tilemaps().size() == 1);
    assert(tilemap.tile(1, 2) == 0x00FFFFFF);
    bool rejected = false;
    try { tilemap.tile(2, 0); } catch (const std::out_of_range&) { rejected = true; }
    assert(rejected);
}

void scene_manager_work() {
    a2e::SceneManager scenes;
    auto& menu = scenes.create_scene("Menu");
    auto& level = scenes.create_scene("Level");
    assert(scenes.size() == 2);
    assert(scenes.active() == &menu);
    assert(scenes.set_active("Level"));
    assert(scenes.active() == &level);
    assert(scenes.find("Menu") == &menu);
    assert(!scenes.set_active("Missing"));
    bool rejected = false;
    try { scenes.create_scene("Level"); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    assert(scenes.destroy_scene("Level"));
    assert(scenes.active() == &menu);
    assert(!scenes.destroy_scene("Missing"));
    scenes.clear();
    assert(scenes.active() == nullptr);
    assert(scenes.active_name().empty());
}

void physics_work() {
    a2e::Scene scene("Physics Scene");
    auto& moving = scene.create_entity("Moving");
    moving.transform().x = -5.0;
    moving.transform().y = -5.0;
    moving.set_collider({20.0, 20.0, 1, 6, false});
    moving.set_rigid_body({10.0, 50.0, true});

    auto& trigger = scene.create_entity("Trigger");
    trigger.transform().x = 5.0;
    trigger.transform().y = 20.0;
    trigger.set_collider({20.0, 20.0, 2, 1, true});

    auto& floor = scene.create_entity("Floor");
    floor.transform().y = 20.0;
    floor.set_collider({20.0, 20.0, 4, 1, false});

    std::vector<a2e::CollisionEvent> collisions;
    a2e::EventBus events;
    int enters = 0;
    int exits = 0;
    events.subscribe<a2e::CollisionEnterEvent>([&enters](const auto&) { ++enters; });
    events.subscribe<a2e::CollisionExitEvent>([&exits](const auto&) { ++exits; });
    a2e::PhysicsSystem physics([&collisions](const a2e::CollisionEvent& event) { collisions.push_back(event); }, 0.0, &events);
    physics.update(scene, a2e::InputState{}, 0.5);

    assert(moving.transform().x == 0.0);
    assert(moving.transform().y == 0.0);
    assert(moving.rigid_body()->velocity_y == 0.0);
    assert(collisions.size() == 2);
    assert(collisions.front().first_entity == moving.id());
    assert(collisions.front().second_entity == trigger.id());
    assert(collisions.front().trigger);
    assert(collisions.front().normal_x == 1.0);
    assert(collisions.front().normal_y == 0.0);
    assert(collisions.front().penetration > 0.0);
    assert(enters == 2);

    physics.update(scene, a2e::InputState{}, 0.0);
    assert(enters == 2);
    moving.transform().x = 100.0;
    moving.transform().y = 100.0;
    physics.update(scene, a2e::InputState{}, 0.0);
    assert(exits == 2);
}

void physics_material_response_works() {
    a2e::Scene scene("Material Scene");
    auto& ball = scene.create_entity("Ball");
    ball.transform().y = 3.0;
    ball.set_collider({10.0, 10.0, 1, 1, false});
    ball.set_rigid_body({10.0, -10.0, true, 0.5, 0.5});

    auto& floor = scene.create_entity("Floor");
    floor.set_collider({10.0, 10.0, 1, 1, false});

    a2e::PhysicsSystem physics({}, 0.0);
    physics.update(scene, a2e::InputState{}, 0.0);

    assert(ball.rigid_body()->velocity_y == 5.0);
    assert(ball.rigid_body()->velocity_x == 5.0);
}

void physics_substeps_reduce_tunneling() {
    a2e::Scene scene("Substep Scene");
    auto& mover = scene.create_entity("Mover");
    mover.set_collider({2.0, 20.0, 1, 1, false});
    mover.set_rigid_body({100.0, 0.0, true});

    auto& wall = scene.create_entity("Wall");
    wall.transform().x = 50.0;
    wall.set_collider({2.0, 20.0, 1, 1, false});

    a2e::PhysicsSystem physics({}, 0.0, nullptr, 0.1);
    physics.update(scene, a2e::InputState{}, 1.0);

    assert(mover.transform().x < wall.transform().x);
    assert(mover.rigid_body()->velocity_x == 0.0);

    bool rejected = false;
    try { a2e::PhysicsSystem invalid({}, 0.0, nullptr, -0.1); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

} // namespace

int main() {
    scene_and_renderer_work();
    resources_and_configuration_work();
    input_and_system_pipeline_work();
    camera_validation_works();
    tilemap_work();
    scene_manager_work();
    physics_work();
    physics_material_response_works();
    physics_substeps_reduce_tunneling();
    std::cout << "Phase 1, Phase 2, and Phase 3 tests passed\n";
}
