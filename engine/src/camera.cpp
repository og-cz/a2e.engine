#include "a2e/camera.hpp"

#include <stdexcept>

namespace a2e {

void Camera::validate() const {
    if (zoom <= 0.0) throw std::invalid_argument("camera zoom must be positive");
}

std::pair<double, double> Camera::world_to_screen(double world_x, double world_y,
                                                   double viewport_width, double viewport_height) const {
    validate();
    return {(world_x - x) * zoom + viewport_width / 2.0,
            (world_y - y) * zoom + viewport_height / 2.0};
}

std::pair<double, double> Camera::screen_to_world(double screen_x, double screen_y,
                                                   double viewport_width, double viewport_height) const {
    validate();
    return {(screen_x - viewport_width / 2.0) / zoom + x,
            (screen_y - viewport_height / 2.0) / zoom + y};
}

} // namespace a2e
