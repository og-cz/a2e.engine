#include "a2e/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace a2e {

void Basic2DRenderer::render(const Scene& scene, RenderTarget& target, const Camera& camera) {
    camera.validate();
    for (const auto& tilemap : scene.tilemaps()) {
        for (int y = 0; y < tilemap->height(); ++y) {
            for (int x = 0; x < tilemap->width(); ++x) {
                const auto color = tilemap->tile(x, y);
                if (color == 0) continue;
                const double size = tilemap->tile_size();
                const double world_x = (static_cast<double>(x) + 0.5) * size;
                const double world_y = (static_cast<double>(y) + 0.5) * size;
                const auto [screen_x, screen_y] = camera.world_to_screen(
                    world_x, world_y, target.width(), target.height());
                const double half_size = size * camera.zoom / 2.0;
                target.fill_rectangle(screen_x - half_size, screen_y - half_size,
                                      screen_x + half_size, screen_y + half_size, color);
            }
        }
    }

    std::vector<const Entity*> ordered_entities;
    ordered_entities.reserve(scene.entities().size());
    for (const auto& entity : scene.entities()) {
        if (entity->active()) ordered_entities.push_back(entity.get());
    }
    std::stable_sort(ordered_entities.begin(), ordered_entities.end(), [](const Entity* left, const Entity* right) {
        const auto* left_renderable = left->renderable();
        const auto* right_renderable = right->renderable();
        const int left_layer = left_renderable ? left_renderable->layer : 0;
        const int right_layer = right_renderable ? right_renderable->layer : 0;
        return left_layer < right_layer;
    });

    for (const auto* entity : ordered_entities) {
        const auto* renderable = entity->renderable();
        if (!renderable || !renderable->visible) continue;
        const auto& transform = entity->transform();
        const double half_width = renderable->width * transform.scale_x / 2.0;
        const double half_height = renderable->height * transform.scale_y / 2.0;
        const auto [screen_x, screen_y] = camera.world_to_screen(
            transform.x, transform.y, target.width(), target.height());
        const double cosine = std::cos(transform.rotation);
        const double sine = std::sin(transform.rotation);
        const double bounds_width = (std::abs(cosine) * half_width + std::abs(sine) * half_height) * camera.zoom;
        const double bounds_height = (std::abs(sine) * half_width + std::abs(cosine) * half_height) * camera.zoom;
        if (screen_x + bounds_width < 0.0 || screen_x - bounds_width > target.width() ||
            screen_y + bounds_height < 0.0 || screen_y - bounds_height > target.height()) {
            continue;
        }
        if (const auto* sprite = entity->sprite(); sprite && sprite->texture && transform.rotation == 0.0) {
            const int source_width = sprite->source_width > 0 ? sprite->source_width : sprite->texture->width();
            const int source_height = sprite->source_height > 0 ? sprite->source_height : sprite->texture->height();
            target.draw_texture(*sprite->texture, sprite->source_x, sprite->source_y,
                                source_width, source_height,
                                screen_x - bounds_width, screen_y - bounds_height,
                                screen_x + bounds_width, screen_y + bounds_height);
            continue;
        }
        if (transform.rotation == 0.0) {
            target.fill_rectangle(screen_x - bounds_width, screen_y - bounds_height,
                                  screen_x + bounds_width, screen_y + bounds_height,
                                  renderable->color);
            continue;
        }

        const std::vector<std::pair<double, double>> corners = {
            {-half_width, -half_height}, {half_width, -half_height},
            {half_width, half_height}, {-half_width, half_height}};
        std::vector<std::pair<double, double>> points;
        points.reserve(corners.size());
        for (const auto& [x, y] : corners) {
            points.emplace_back(screen_x + (x * cosine - y * sine) * camera.zoom,
                                screen_y + (x * sine + y * cosine) * camera.zoom);
        }
        target.fill_polygon(points, renderable->color);
    }
}

} // namespace a2e
