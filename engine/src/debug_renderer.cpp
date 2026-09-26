#include "a2e/debug_renderer.hpp"

#include "a2e/agent.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace a2e {
namespace {

constexpr std::uint32_t collider_color = 0x0050E36A;
constexpr std::uint32_t trigger_color = 0x00F5D547;
constexpr std::uint32_t obstacle_color = 0x00FF9A3C;
constexpr std::uint32_t path_color = 0x003CD6FF;
constexpr std::uint32_t vision_color = 0x00E8E8E8;
constexpr std::uint32_t seen_color = 0x00FF4D4D;
constexpr std::uint32_t heard_color = 0x00FFA23C;

class Overlay {
public:
    Overlay(RenderTarget& target, const Camera& camera, std::size_t& count)
        : target_(target), camera_(camera), count_(count) {}

    std::pair<double, double> screen(double x, double y) const {
        return camera_.world_to_screen(x, y, target_.width(), target_.height());
    }

    void line(double x0, double y0, double x1, double y1, std::uint32_t color) {
        const auto [sx0, sy0] = screen(x0, y0);
        const auto [sx1, sy1] = screen(x1, y1);
        draw_debug_line(target_, sx0, sy0, sx1, sy1, color);
        ++count_;
    }

    void box(double left, double top, double right, double bottom, std::uint32_t color) {
        line(left, top, right, top, color);
        line(right, top, right, bottom, color);
        line(right, bottom, left, bottom, color);
        line(left, bottom, left, top, color);
    }

    // Fixed screen-size marker centered on a world point.
    void marker(double x, double y, double half_size, std::uint32_t color) {
        const auto [sx, sy] = screen(x, y);
        target_.fill_rectangle(sx - half_size, sy - half_size, sx + half_size, sy + half_size, color);
        ++count_;
    }

private:
    RenderTarget& target_;
    const Camera& camera_;
    std::size_t& count_;
};

} // namespace

void draw_debug_line(RenderTarget& target, double x0, double y0, double x1, double y1, std::uint32_t color,
                     double thickness) {
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double length = std::sqrt(dx * dx + dy * dy);
    const double half = thickness / 2.0;
    if (length < 1e-9) {
        target.fill_rectangle(x0 - half, y0 - half, x0 + half, y0 + half, color);
        return;
    }
    const double nx = -dy / length * half;
    const double ny = dx / length * half;
    target.fill_polygon({{x0 + nx, y0 + ny}, {x1 + nx, y1 + ny}, {x1 - nx, y1 - ny}, {x0 - nx, y0 - ny}}, color);
}

DebugRenderer::DebugRenderer(std::unique_ptr<Renderer> scene_renderer, const NavigationGrid* grid)
    : scene_renderer_(std::move(scene_renderer)), grid_(grid) {
    if (!scene_renderer_) throw std::invalid_argument("debug renderer needs a scene renderer");
}

void DebugRenderer::render(const Scene& scene, RenderTarget& target, const Camera& camera) {
    scene_renderer_->render(scene, target, camera);
    overlay_primitives_ = 0;
    if (!enabled_) return;
    Overlay overlay(target, camera, overlay_primitives_);

    if (grid_ && options_.obstacles) {
        const double size = grid_->cell_size();
        for (int y = 0; y < grid_->height(); ++y) {
            for (int x = 0; x < grid_->width(); ++x) {
                if (!grid_->has_obstacle({x, y})) continue;
                const auto [center_x, center_y] = grid_->cell_center({x, y});
                overlay.box(center_x - size / 2.0 + 2.0, center_y - size / 2.0 + 2.0,
                            center_x + size / 2.0 - 2.0, center_y + size / 2.0 - 2.0, obstacle_color);
            }
        }
    }

    for (const auto& entity : scene.entities()) {
        if (!entity->active()) continue;
        const auto& transform = entity->transform();

        if (const auto* collider = entity->collider(); collider && options_.colliders) {
            const double half_width = collider->width * transform.scale_x / 2.0;
            const double half_height = collider->height * transform.scale_y / 2.0;
            overlay.box(transform.x - half_width, transform.y - half_height, transform.x + half_width,
                        transform.y + half_height, collider->trigger ? trigger_color : collider_color);
        }

        if (const auto* navigation = entity->get_component<NavigationAgent>();
            navigation && options_.navigation_paths && navigation->status == NavigationStatus::Moving) {
            double from_x = transform.x;
            double from_y = transform.y;
            for (auto index = navigation->next_waypoint; index < navigation->waypoints.size(); ++index) {
                const auto [to_x, to_y] = navigation->waypoints[index];
                overlay.line(from_x, from_y, to_x, to_y, path_color);
                from_x = to_x;
                from_y = to_y;
            }
            overlay.marker(from_x, from_y, 4.0, path_color);
        }

        const auto* agent = entity->get_component<Agent>();
        if (!agent) continue;
        if (options_.agent_vision) {
            const double facing = std::atan2(agent->facing_y, agent->facing_x);
            for (const auto& sensor : agent->sensors()) {
                const auto* vision = dynamic_cast<const VisionSensor*>(sensor.get());
                if (!vision) continue;
                constexpr int arc_segments = 12;
                const double start = facing - vision->field_of_view() / 2.0;
                const double step = vision->field_of_view() / arc_segments;
                const auto point = [&](double angle) {
                    return std::make_pair(transform.x + std::cos(angle) * vision->range(),
                                          transform.y + std::sin(angle) * vision->range());
                };
                auto previous = point(start);
                overlay.line(transform.x, transform.y, previous.first, previous.second, vision_color);
                for (int segment = 1; segment <= arc_segments; ++segment) {
                    const auto next = point(start + step * segment);
                    overlay.line(previous.first, previous.second, next.first, next.second, vision_color);
                    previous = next;
                }
                overlay.line(previous.first, previous.second, transform.x, transform.y, vision_color);
            }
        }
        if (options_.agent_memory) {
            for (const auto& record : agent->memory.records()) {
                const bool seen = record.observation.kind == "seen";
                const double half_size = 2.0 + 4.0 * record.strength;
                overlay.marker(record.observation.x, record.observation.y, half_size, seen ? seen_color : heard_color);
            }
        }
    }
}

} // namespace a2e
