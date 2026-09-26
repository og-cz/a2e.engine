#include "a2e/perception.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace a2e {

bool has_line_of_sight(const NavigationGrid& grid, double from_x, double from_y, double to_x, double to_y) {
    const double dx = to_x - from_x;
    const double dy = to_y - from_y;
    const double distance = std::sqrt(dx * dx + dy * dy);
    const double step = grid.cell_size() / 4.0;
    const int samples = std::max(1, static_cast<int>(std::ceil(distance / step)));
    for (int sample = 0; sample <= samples; ++sample) {
        const double t = static_cast<double>(sample) / samples;
        const auto cell = grid.world_to_cell(from_x + dx * t, from_y + dy * t);
        if (cell && !grid.is_walkable(*cell)) return false;
    }
    return true;
}

VisionSensor::VisionSensor(double range, double field_of_view_radians)
    : range_(range), field_of_view_(field_of_view_radians) {
    if (range <= 0.0) throw std::invalid_argument("vision range must be positive");
    if (field_of_view_radians <= 0.0 || field_of_view_radians > 6.283185307179586) {
        throw std::invalid_argument("field of view must be in (0, 2*pi]");
    }
}

void VisionSensor::sense(const SensorContext& context, std::vector<Observation>& observations) const {
    const auto& origin = context.self.transform();
    const double facing_length = std::sqrt(context.facing_x * context.facing_x + context.facing_y * context.facing_y);
    const double cos_half_view = std::cos(field_of_view_ / 2.0);
    for (const auto& entity : context.scene.entities()) {
        if (entity.get() == &context.self || !entity->active()) continue;
        const auto* perceivable = entity->get_component<Perceivable>();
        if (!perceivable || !perceivable->detectable) continue;
        const double dx = entity->transform().x - origin.x;
        const double dy = entity->transform().y - origin.y;
        const double distance = std::sqrt(dx * dx + dy * dy);
        if (distance > range_) continue;
        if (distance > 1e-9 && facing_length > 1e-9) {
            const double alignment = (dx * context.facing_x + dy * context.facing_y) / (distance * facing_length);
            if (alignment < cos_half_view) continue;
        }
        if (context.occlusion &&
            !has_line_of_sight(*context.occlusion, origin.x, origin.y, entity->transform().x, entity->transform().y)) {
            continue;
        }
        observations.push_back({entity->id(), "seen", perceivable->tag, entity->transform().x, entity->transform().y,
                                context.time, 1.0 - 0.5 * distance / range_});
    }
}

HearingSensor::HearingSensor(double sensitivity) : sensitivity_(sensitivity) {
    if (sensitivity <= 0.0) throw std::invalid_argument("hearing sensitivity must be positive");
}

void HearingSensor::sense(const SensorContext& context, std::vector<Observation>& observations) const {
    const auto& origin = context.self.transform();
    for (const auto& sound : context.sounds) {
        if (sound.source == context.self.id()) continue;
        const double radius = sound.loudness * sensitivity_;
        const double dx = sound.x - origin.x;
        const double dy = sound.y - origin.y;
        const double distance = std::sqrt(dx * dx + dy * dy);
        if (radius <= 0.0 || distance > radius) continue;
        observations.push_back({sound.source, "heard", sound.tag, sound.x, sound.y, context.time,
                                1.0 - 0.7 * distance / radius});
    }
}

ProximitySensor::ProximitySensor(double range) : range_(range) {
    if (range <= 0.0) throw std::invalid_argument("proximity range must be positive");
}

void ProximitySensor::sense(const SensorContext& context, std::vector<Observation>& observations) const {
    const auto& origin = context.self.transform();
    for (const auto& entity : context.scene.entities()) {
        if (entity.get() == &context.self || !entity->active()) continue;
        const auto* perceivable = entity->get_component<Perceivable>();
        if (!perceivable || !perceivable->detectable) continue;
        const double dx = entity->transform().x - origin.x;
        const double dy = entity->transform().y - origin.y;
        if (dx * dx + dy * dy > range_ * range_) continue;
        observations.push_back({entity->id(), "nearby", perceivable->tag, entity->transform().x,
                                entity->transform().y, context.time, 1.0});
    }
}

} // namespace a2e
