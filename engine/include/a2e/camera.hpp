#pragma once

#include <utility>

namespace a2e {

struct Camera {
    double x = 0.0;
    double y = 0.0;
    double zoom = 1.0;

    void validate() const;
    std::pair<double, double> world_to_screen(double world_x, double world_y,
                                              double viewport_width, double viewport_height) const;
    std::pair<double, double> screen_to_world(double screen_x, double screen_y,
                                              double viewport_width, double viewport_height) const;
};

} // namespace a2e
