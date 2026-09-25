// Tests rely on assert, so keep it active even in Release builds.
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "a2e/a2e.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
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
    void draw_texture(const a2e::Texture&, int, int, int, int, double, double, double, double) override { ++texture_count; }
    void present() override { ++present_count; }
    int clear_count = 0;
    int rectangle_count = 0;
    int polygon_count = 0;
    int texture_count = 0;
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
    void draw_texture(const a2e::Texture&, int, int, int, int, double, double, double, double) override {}

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

class FixedRecordingSystem final : public a2e::FixedUpdateSystem {
public:
    explicit FixedRecordingSystem(int& calls) : calls_(calls) {}
    void fixed_update(a2e::Scene&, const a2e::InputState&, double delta_seconds) override {
        ++calls_;
        assert(delta_seconds > 0.0);
    }

private:
    int& calls_;
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
    a2e::EngineConfig invalid_fixed;
    invalid_fixed.fixed_update_hz = -1.0;
    rejected = false;
    try { invalid_fixed.validate(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    a2e::ResourceManager resources;
    resources.register_resource("number", std::make_shared<int>(42));
    assert(*resources.get<int>("number") == 42);
    assert(resources.size() == 1);

    auto texture = std::make_shared<a2e::Texture>(2, 2, std::vector<std::uint32_t>{
        0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFFFF});
    resources.register_resource("test_texture", texture);
    assert(resources.get<a2e::Texture>("test_texture")->pixel(1, 0) == 0x0000FF00);
    a2e::Scene sprite_scene("Sprites");
    auto& sprite_entity = sprite_scene.create_entity("Sprite Entity");
    sprite_entity.set_renderable({32.0, 32.0, 0x00FFFFFF, 0, true});
    sprite_entity.set_sprite({texture, 0, 0, 2, 2});
    assert(sprite_entity.sprite()->texture == texture);
    a2e::Basic2DRenderer sprite_renderer;
    FakeTarget sprite_target;
    sprite_renderer.render(sprite_scene, sprite_target, a2e::Camera{0.0, 0.0, 1.0});
    assert(sprite_target.texture_count == 1);

    a2e::Clock clock;
    assert(clock.frame_count() == 0);
    clock.tick();
    assert(clock.frame_count() == 1);
    assert(clock.delta_seconds() >= 0.0);
    assert(clock.total_seconds() >= clock.delta_seconds());
    clock.reset();
    assert(clock.frame_count() == 0);
    assert(clock.total_seconds() == 0.0);

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
    input.set_mouse_wheel(120);
    input.set_mouse_wheel(-40);
    assert(input.mouse_wheel_delta() == 80);
    input.begin_frame();
    assert(input.mouse_wheel_delta() == 0);

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
    a2e::EngineConfig pipeline_config;
    pipeline_config.fixed_update_hz = 100000.0;
    a2e::Application application(
        pipeline_config, a2e::Scene("Pipeline Scene"),
        std::make_unique<FakeRenderer>(), std::move(window));
    application.add_system(std::make_unique<RecordingSystem>(order, "first"));
    application.add_system(std::make_unique<RecordingSystem>(order, "second"));
    int fixed_calls = 0;
    application.add_fixed_system(std::make_unique<FixedRecordingSystem>(fixed_calls));
    application.run();
    assert((order == std::vector<std::string>{"first", "second"}));
    assert(fixed_calls > 0);
    assert(application.clock().frame_count() == 1);
    assert(application.clock().delta_seconds() >= 0.0);
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

void player_controller_works() {
    a2e::Scene scene("Controller Scene");
    auto& walker = scene.create_entity("Walker");
    a2e::PlayerController controller(walker.id(), 100.0);

    a2e::InputState input;
    input.set_key(a2e::Key::D, true);
    controller.update(scene, input, 0.5);
    assert(walker.transform().x == 50.0);
    assert(walker.transform().y == 0.0);

    input.set_key(a2e::Key::S, true);
    controller.update(scene, input, 1.0);
    const double diagonal = 100.0 / std::sqrt(2.0);
    assert(std::abs(walker.transform().x - (50.0 + diagonal)) < 1e-9);
    assert(std::abs(walker.transform().y - diagonal) < 1e-9);

    walker.set_rigid_body({0.0, 0.0, true});
    input.set_key(a2e::Key::S, false);
    const double before_x = walker.transform().x;
    controller.update(scene, input, 1.0);
    assert(walker.transform().x == before_x);
    assert(walker.rigid_body()->velocity_x == 100.0);
    assert(walker.rigid_body()->velocity_y == 0.0);
    input.set_key(a2e::Key::D, false);
    controller.update(scene, input, 1.0);
    assert(walker.rigid_body()->velocity_x == 0.0);

    a2e::GamepadState gamepad;
    controller.set_gamepad(&gamepad);
    gamepad.set_left_stick(0.1, -0.5);
    controller.update(scene, input, 1.0);
    assert(walker.rigid_body()->velocity_x == 0.0);
    assert(walker.rigid_body()->velocity_y == -50.0);

    bool rejected = false;
    try { controller.set_speed(-1.0); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

void animation_works() {
    const auto clip = a2e::AnimationClip::from_grid(8, 8, 2, 1, 3, 0.1);
    assert(clip.frames.size() == 3);
    assert(clip.frames[0].source_x == 8 && clip.frames[0].source_y == 0);
    assert(clip.frames[1].source_x == 0 && clip.frames[1].source_y == 8);
    assert(std::abs(clip.length() - 0.3) < 1e-9);

    a2e::Scene scene("Animation Scene");
    auto& hero = scene.create_entity("Hero");
    auto texture = std::make_shared<a2e::Texture>(16, 16, std::vector<std::uint32_t>(256, 0x00FFFFFF));
    hero.set_sprite({texture});
    auto& animator = hero.add_component<a2e::Animator>();
    animator.add_state("walk", clip);
    auto once = clip;
    once.loop = false;
    animator.add_state("attack", once);
    assert(animator.state() == "walk");

    a2e::EventBus events;
    std::string finished_state;
    events.subscribe<a2e::AnimationFinishedEvent>([&finished_state](const a2e::AnimationFinishedEvent& event) {
        finished_state = event.state;
    });
    a2e::AnimationSystem system(&events);
    system.update(scene, a2e::InputState{}, 0.15);
    assert(animator.frame_index() == 1);
    assert(hero.sprite()->source_x == 0 && hero.sprite()->source_y == 8);
    system.update(scene, a2e::InputState{}, 0.2);
    assert(animator.frame_index() == 0);

    assert(animator.play("attack"));
    assert(!animator.play("missing"));
    system.update(scene, a2e::InputState{}, 0.5);
    assert(animator.finished());
    assert(animator.frame_index() == 2);
    assert(finished_state == "attack");
    finished_state.clear();
    system.update(scene, a2e::InputState{}, 0.5);
    assert(finished_state.empty());
    assert(animator.play("attack", true));
    assert(!animator.finished() && animator.frame_index() == 0);

    bool rejected = false;
    try { animator.add_state("empty", a2e::AnimationClip{}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

class RecordingAudioBackend final : public a2e::AudioBackend {
public:
    struct Play {
        double volume;
        bool loop;
    };
    a2e::SoundHandle play(std::shared_ptr<const a2e::AudioClip>, double volume, bool loop) override {
        plays.push_back({volume, loop});
        playing.push_back(next);
        return next++;
    }
    void stop(a2e::SoundHandle handle) override {
        playing.erase(std::remove(playing.begin(), playing.end(), handle), playing.end());
    }
    void stop_all() override { playing.clear(); }
    bool is_playing(a2e::SoundHandle handle) const override {
        return std::find(playing.begin(), playing.end(), handle) != playing.end();
    }
    void update() override { ++updates; }

    std::vector<Play> plays;
    std::vector<a2e::SoundHandle> playing;
    a2e::SoundHandle next = 1;
    int updates = 0;
};

void audio_works() {
    const auto tone = std::make_shared<a2e::AudioClip>(a2e::AudioClip::tone(440.0, 0.5, 0.5, 8000));
    assert(tone->samples.size() == 4000);
    assert(std::abs(tone->duration() - 0.5) < 1e-9);
    assert(tone->samples.front() == 0);
    const auto loudest = *std::max_element(tone->samples.begin(), tone->samples.end());
    assert(loudest > 16000 && loudest <= 16384);

    a2e::NullAudioBackend silent;
    const auto one_shot = silent.play(tone, 1.0, false);
    const auto looping = silent.play(tone, 1.0, true);
    assert(!silent.is_playing(one_shot));
    assert(silent.is_playing(looping));
    silent.stop(looping);
    assert(!silent.is_playing(looping));

    auto backend = std::make_unique<RecordingAudioBackend>();
    auto& device = *backend;
    a2e::AudioManager audio(std::move(backend));
    audio.set_master_volume(0.5);
    audio.set_sound_volume(2.0);
    assert(audio.sound_volume() == 1.0);
    audio.play_sound(tone, 0.5);
    assert(device.plays.back().volume == 0.25);
    assert(!device.plays.back().loop);

    audio.set_music_volume(0.8);
    const auto first_music = audio.play_music(tone);
    assert(device.plays.back().loop);
    assert(std::abs(device.plays.back().volume - 0.4) < 1e-9);
    const auto second_music = audio.play_music(tone);
    assert(!audio.is_playing(first_music));
    assert(audio.is_playing(second_music));
    audio.stop_music();
    assert(audio.music() == 0);
    assert(!audio.is_playing(second_music));

    a2e::Scene scene("Audio Scene");
    auto& speaker = scene.create_entity("Speaker");
    auto& source = speaker.add_component<a2e::AudioSource>();
    source.clip = tone;
    source.loop = true;
    source.play();
    a2e::AudioSystem system(audio);
    system.update(scene, a2e::InputState{}, 0.016);
    assert(source.handle != 0);
    assert(audio.is_playing(source.handle));
    assert(!source.play_requested);
    assert(device.updates == 1);
    const auto speaker_handle = source.handle;
    speaker.set_active(false);
    system.update(scene, a2e::InputState{}, 0.016);
    assert(!audio.is_playing(speaker_handle));
    assert(source.handle == 0);
}

void pathfinding_works() {
    a2e::NavigationGrid grid(5, 5, 10.0);
    grid.allow_diagonal = false;
    auto path = a2e::find_path(grid, {0, 0}, {4, 0});
    assert(path.size() == 5);
    assert(path.front() == (a2e::GridCell{0, 0}) && path.back() == (a2e::GridCell{4, 0}));

    for (int y = 0; y < 4; ++y) grid.set_walkable({2, y}, false);
    path = a2e::find_path(grid, {0, 0}, {4, 0});
    assert(path.size() == 13);
    for (const auto& cell : path) assert(!grid.is_blocked(cell));
    assert(a2e::find_path(grid, {0, 0}, {4, 0}) == path);

    grid.set_walkable({2, 4}, false);
    assert(a2e::find_path(grid, {0, 0}, {4, 0}).empty());
    assert(a2e::find_path(grid, {0, 0}, {2, 0}).empty());
    assert(a2e::find_path(grid, {1, 1}, {1, 1}).size() == 1);

    a2e::NavigationGrid diagonal(3, 3, 1.0);
    assert(a2e::find_path(diagonal, {0, 0}, {2, 2}).size() == 3);
    diagonal.set_walkable({1, 0}, false);
    diagonal.set_walkable({0, 1}, false);
    assert(a2e::find_path(diagonal, {0, 0}, {2, 2}).empty());

    a2e::NavigationGrid costly(3, 3, 1.0);
    costly.allow_diagonal = false;
    costly.set_cost({1, 0}, 10.0);
    path = a2e::find_path(costly, {0, 0}, {2, 0});
    assert(path.size() == 5);
    assert(std::find(path.begin(), path.end(), a2e::GridCell{1, 0}) == path.end());

    a2e::TileMap tiles(3, 2, 16.0);
    tiles.set_tile(1, 0, 0x00FF0000);
    const auto from_tiles = a2e::NavigationGrid::from_tilemap(tiles, [](std::uint32_t tile) { return tile == 0; });
    assert(from_tiles.cell_size() == 16.0);
    assert(!from_tiles.is_walkable({1, 0}) && from_tiles.is_walkable({1, 1}));

    a2e::NavigationGrid obstacles(4, 4, 10.0);
    obstacles.add_obstacle_rect(12.0, 12.0, 28.0, 20.0);
    assert(obstacles.has_obstacle({1, 1}) && obstacles.has_obstacle({2, 1}));
    assert(!obstacles.has_obstacle({1, 2}) && !obstacles.has_obstacle({3, 1}));
    assert(obstacles.is_walkable({1, 1}) && obstacles.is_blocked({1, 1}));
    obstacles.clear_obstacles();
    assert(!obstacles.is_blocked({1, 1}));
    assert(!obstacles.is_blocked({0, 0}) && obstacles.is_blocked({-1, 0}));
    assert(obstacles.world_to_cell(15.0, 39.9) == (a2e::GridCell{1, 3}));
    assert(!obstacles.world_to_cell(-0.1, 5.0));
    assert(obstacles.cell_center({1, 3}) == std::make_pair(15.0, 35.0));

    bool rejected = false;
    try { costly.set_cost({0, 0}, 0.5); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

void navigation_agents_work() {
    a2e::NavigationGrid grid(6, 3, 10.0);
    grid.allow_diagonal = false;
    for (int y = 0; y < 2; ++y) grid.set_walkable({3, y}, false);

    a2e::Scene scene("Navigation Scene");
    auto& npc = scene.create_entity("NPC");
    npc.transform().x = 5.0;
    npc.transform().y = 5.0;
    auto& agent = npc.add_component<a2e::NavigationAgent>();
    agent.speed = 20.0;
    agent.set_destination(55.0, 5.0);

    a2e::EventBus events;
    int arrived = 0;
    int failed = 0;
    events.subscribe<a2e::NavigationArrivedEvent>([&arrived](const a2e::NavigationArrivedEvent&) { ++arrived; });
    events.subscribe<a2e::NavigationFailedEvent>([&failed](const a2e::NavigationFailedEvent&) { ++failed; });
    a2e::NavigationSystem navigation(grid, &events);

    navigation.update(scene, a2e::InputState{}, 0.25);
    assert(agent.status == a2e::NavigationStatus::Moving);
    assert(npc.transform().x == 10.0 && npc.transform().y == 5.0);
    for (int frame = 0; frame < 100 && agent.status == a2e::NavigationStatus::Moving; ++frame) {
        navigation.update(scene, a2e::InputState{}, 0.25);
        const auto cell = grid.world_to_cell(npc.transform().x, npc.transform().y);
        assert(cell && grid.is_walkable(*cell));
    }
    assert(agent.status == a2e::NavigationStatus::Arrived);
    assert(std::abs(npc.transform().x - 55.0) <= agent.arrival_distance);
    assert(std::abs(npc.transform().y - 5.0) <= agent.arrival_distance);
    assert(arrived == 1 && failed == 0);

    agent.set_destination(5.0, 5.0);
    navigation.update(scene, a2e::InputState{}, 0.1);
    assert(agent.status == a2e::NavigationStatus::Moving);
    grid.add_obstacle({3, 2});
    navigation.update(scene, a2e::InputState{}, 0.1);
    assert(agent.status == a2e::NavigationStatus::Unreachable);
    assert(failed == 1);

    grid.clear_obstacles();
    auto& runner = scene.create_entity("Runner");
    runner.transform().x = 5.0;
    runner.transform().y = 25.0;
    runner.set_rigid_body({0.0, 0.0, true});
    runner.add_component<a2e::NavigationAgent>().set_destination(55.0, 25.0);
    npc.remove_component<a2e::NavigationAgent>();
    navigation.update(scene, a2e::InputState{}, 0.1);
    assert(runner.transform().x == 5.0);
    assert(runner.rigid_body()->velocity_x == 120.0);
    assert(runner.rigid_body()->velocity_y == 0.0);
    runner.get_component<a2e::NavigationAgent>()->stop();
    assert(runner.get_component<a2e::NavigationAgent>()->status == a2e::NavigationStatus::Idle);
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
    player_controller_works();
    animation_works();
    audio_works();
    pathfinding_works();
    navigation_agents_work();
    std::cout << "Engine tests passed\n";
}
