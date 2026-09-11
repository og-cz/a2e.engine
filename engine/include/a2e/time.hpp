#pragma once

#include <chrono>
#include <cstdint>

namespace a2e {

class Clock {
public:
    explicit Clock(double max_delta_seconds = 0.25);
    void reset();
    double tick();
    double delta_seconds() const { return last_delta_seconds_; }
    double total_seconds() const { return total_seconds_; }
    std::uint64_t frame_count() const { return frame_count_; }

private:
    double max_delta_seconds_;
    std::chrono::steady_clock::time_point last_tick_;
    double last_delta_seconds_ = 0.0;
    double total_seconds_ = 0.0;
    std::uint64_t frame_count_ = 0;
};

} // namespace a2e
