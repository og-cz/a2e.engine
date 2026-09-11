#pragma once

#include "a2e/camera.hpp"
#include "a2e/config.hpp"
#include "a2e/events.hpp"
#include "a2e/input.hpp"
#include "a2e/renderer.hpp"
#include "a2e/resources.hpp"
#include "a2e/scene.hpp"
#include "a2e/time.hpp"
#include "a2e/update_system.hpp"
#include "a2e/window.hpp"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace a2e {

class Application {
public:
    Application(EngineConfig config, Scene scene, std::unique_ptr<Renderer> renderer,
                std::unique_ptr<Window> window = nullptr);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void set_update_callback(std::function<void(double)> callback);
    void add_system(std::unique_ptr<UpdateSystem> system);
    int run();
    void stop();
    bool is_running() const { return running_; }
    Scene& scene() { return scene_; }
    ResourceManager& resources() { return resources_; }
    EventBus& events() { return events_; }
    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }
    const InputState& input() const { return window_->input(); }

private:
    EngineConfig config_;
    Scene scene_;
    Camera camera_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Window> window_;
    ResourceManager resources_;
    EventBus events_;
    Clock clock_;
    std::function<void(double)> update_callback_;
    std::vector<std::unique_ptr<UpdateSystem>> systems_;
    bool running_ = false;
};

} // namespace a2e
