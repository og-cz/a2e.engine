#pragma once

#include "a2e/input.hpp"
#include "a2e/scene.hpp"

namespace a2e {

class UpdateSystem {
public:
    virtual ~UpdateSystem() = default;
    virtual void update(Scene& scene, const InputState& input, double delta_seconds) = 0;
};

} // namespace a2e
