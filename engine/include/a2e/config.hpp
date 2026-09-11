#pragma once

#include <cstdint>
#include <string>

namespace a2e {

struct EngineConfig {
    std::int32_t width = 800;
    std::int32_t height = 450;
    std::string title = "A2E";
    std::int32_t target_fps = 60;
    std::uint32_t background_color = 0x0018212B;
    double max_delta_seconds = 0.25;

    void validate() const;
};

} // namespace a2e
