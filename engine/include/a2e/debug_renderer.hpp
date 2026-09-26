#pragma once

#include "a2e/navigation.hpp"
#include "a2e/renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace a2e {

struct DebugDrawOptions {
    bool colliders = true;
    bool obstacles = true;
    bool navigation_paths = true;
    bool agent_vision = true;
    bool agent_memory = true;
};

// Draws a thick screen-space line through the generic polygon primitive.
void draw_debug_line(RenderTarget& target, double x0, double y0, double x1, double y1, std::uint32_t color,
                     double thickness = 2.0);

// Wraps another renderer and overlays engine state: colliders, dynamic obstacles, navigation paths,
// agent vision cones, and remembered observations. It only reads scene data.
class DebugRenderer final : public Renderer {
public:
    explicit DebugRenderer(std::unique_ptr<Renderer> scene_renderer, const NavigationGrid* grid = nullptr);
    void render(const Scene& scene, RenderTarget& target, const Camera& camera) override;

    bool enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    void toggle() { enabled_ = !enabled_; }
    DebugDrawOptions& options() { return options_; }
    // Number of overlay primitives issued by the last render, for tests and diagnostics.
    std::size_t overlay_primitives() const { return overlay_primitives_; }

private:
    std::unique_ptr<Renderer> scene_renderer_;
    const NavigationGrid* grid_;
    DebugDrawOptions options_;
    bool enabled_ = true;
    std::size_t overlay_primitives_ = 0;
};

} // namespace a2e
