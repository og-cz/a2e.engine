#include "a2e/time.hpp"

#include <algorithm>
#include <stdexcept>

namespace a2e {

Clock::Clock(double max_delta_seconds)
    : max_delta_seconds_(max_delta_seconds), last_tick_(std::chrono::steady_clock::now()) {
    if (max_delta_seconds <= 0.0) throw std::invalid_argument("max_delta_seconds must be positive");
}

void Clock::reset() { last_tick_ = std::chrono::steady_clock::now(); }

double Clock::tick() {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double>(now - last_tick_).count();
    last_tick_ = now;
    return std::clamp(elapsed, 0.0, max_delta_seconds_);
}

} // namespace a2e
