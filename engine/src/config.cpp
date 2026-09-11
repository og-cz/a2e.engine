#include "a2e/config.hpp"

#include <stdexcept>

namespace a2e {

void EngineConfig::validate() const {
    if (width <= 0 || height <= 0) throw std::invalid_argument("window dimensions must be positive");
    if (target_fps <= 0) throw std::invalid_argument("target_fps must be positive");
    if (max_delta_seconds <= 0.0) throw std::invalid_argument("max_delta_seconds must be positive");
}

} // namespace a2e
