#include "a2e/application.hpp"
#include "a2e/logging.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace a2e {

Application::Application(EngineConfig config, Scene scene, std::unique_ptr<Renderer> renderer,
                         std::unique_ptr<Window> window)
    : config_(std::move(config)), scene_(std::move(scene)), renderer_(std::move(renderer)),
            window_(std::move(window)), camera_{config_.width / 2.0, config_.height / 2.0, 1.0},
            clock_(config_.max_delta_seconds) {
    config_.validate();
        camera_.validate();
    if (!renderer_) throw std::invalid_argument("renderer cannot be null");
}

Application::~Application() { stop(); }

void Application::set_update_callback(std::function<void(double)> callback) {
    update_callback_ = std::move(callback);
}

void Application::add_system(std::unique_ptr<UpdateSystem> system) {
    if (!system) throw std::invalid_argument("update system cannot be null");
    systems_.push_back(std::move(system));
}

int Application::run() {
    if (!window_) window_ = create_window(config_.width, config_.height, config_.title, config_.background_color);
    running_ = true;
    clock_.reset();
    log_info("started scene '" + scene_.name() + "'");

    const auto target_frame = std::chrono::duration<double>(1.0 / config_.target_fps);
    while (running_ && window_->is_open()) {
        const auto frame_start = std::chrono::steady_clock::now();
        if (!window_->process_events()) break;
        const double delta_seconds = clock_.tick();
        if (update_callback_) update_callback_(delta_seconds);
        for (const auto& system : systems_) {
            system->update(scene_, window_->input(), delta_seconds);
        }
        if (running_) {
            window_->clear(config_.background_color);
            renderer_->render(scene_, *window_, camera_);
            window_->present();
        }
        const auto elapsed = std::chrono::steady_clock::now() - frame_start;
        if (elapsed < target_frame) std::this_thread::sleep_for(target_frame - elapsed);
    }
    stop();
    return 0;
}

void Application::stop() {
    if (!running_ && (!window_ || !window_->is_open())) return;
    running_ = false;
    if (window_) window_->close();
    log_info("shut down cleanly");
}

} // namespace a2e
