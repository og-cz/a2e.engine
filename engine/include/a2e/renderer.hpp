#pragma once

#include "a2e/camera.hpp"
#include "a2e/scene.hpp"
#include "a2e/window.hpp"

namespace a2e {

class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void render(const Scene& scene, RenderTarget& target, const Camera& camera) = 0;
};

class Basic2DRenderer final : public Renderer {
public:
    void render(const Scene& scene, RenderTarget& target, const Camera& camera) override;
};

} // namespace a2e
