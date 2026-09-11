#pragma once

#include <chrono>

namespace a2e {

class Clock {
public:
    explicit Clock(double max_delta_seconds = 0.25);
    void reset();
    double tick();

private:
    double max_delta_seconds_;
    std::chrono::steady_clock::time_point last_tick_;
};

} // namespace a2e
