#pragma once

#include "a2e/navigation.hpp"
#include "a2e/scene.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace a2e {

// Something an agent noticed. Agents never read the world directly; they reason about observations.
struct Observation {
    std::uint64_t source = 0;
    std::string kind;
    std::string tag;
    double x = 0.0;
    double y = 0.0;
    double time = 0.0;
    double confidence = 1.0;
};

// Entity component marking something sensors may notice. Entities without it are invisible to agents.
struct Perceivable {
    std::string tag;
    bool detectable = true;
};

// Published by gameplay (footsteps, doors, explosions). Loudness is the radius at which it can be heard.
struct SoundProducedEvent {
    std::uint64_t source = 0;
    double x = 0.0;
    double y = 0.0;
    double loudness = 100.0;
    std::string tag;
};

struct SensorContext {
    const Scene& scene;
    const Entity& self;
    double facing_x;
    double facing_y;
    double time;
    const std::vector<SoundProducedEvent>& sounds;
    // Optional. Statically unwalkable cells block line of sight when provided.
    const NavigationGrid* occlusion = nullptr;
};

class Sensor {
public:
    virtual ~Sensor() = default;
    virtual void sense(const SensorContext& context, std::vector<Observation>& observations) const = 0;
};

// Sees Perceivable entities inside a cone. Confidence falls with distance.
class VisionSensor final : public Sensor {
public:
    VisionSensor(double range, double field_of_view_radians);
    void sense(const SensorContext& context, std::vector<Observation>& observations) const override;
    double range() const { return range_; }
    double field_of_view() const { return field_of_view_; }

private:
    double range_;
    double field_of_view_;
};

// Hears SoundProducedEvents within their loudness radius, scaled by sensitivity.
class HearingSensor final : public Sensor {
public:
    explicit HearingSensor(double sensitivity = 1.0);
    void sense(const SensorContext& context, std::vector<Observation>& observations) const override;

private:
    double sensitivity_;
};

// Notices Perceivable entities within a radius regardless of facing or walls, like touch or presence.
class ProximitySensor final : public Sensor {
public:
    explicit ProximitySensor(double range);
    void sense(const SensorContext& context, std::vector<Observation>& observations) const override;

private:
    double range_;
};

// True when no statically unwalkable cell lies on the segment. Cells outside the grid never block.
bool has_line_of_sight(const NavigationGrid& grid, double from_x, double from_y, double to_x, double to_y);

} // namespace a2e
