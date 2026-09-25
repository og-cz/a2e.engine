#pragma once

#include "a2e/events.hpp"
#include "a2e/tilemap.hpp"
#include "a2e/update_system.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace a2e {

struct GridCell {
    int x = 0;
    int y = 0;
    bool operator==(const GridCell& other) const { return x == other.x && y == other.y; }
    bool operator!=(const GridCell& other) const { return !(*this == other); }
};

// Walkability grid in world space. Static walkability and dynamic obstacles are stored
// separately so obstacles can be cleared and re-marked every frame without losing the map.
class NavigationGrid {
public:
    NavigationGrid(int width, int height, double cell_size, double origin_x = 0.0, double origin_y = 0.0);
    // Builds a grid aligned with a tilemap; the predicate decides which tile values are walkable.
    static NavigationGrid from_tilemap(const TileMap& tilemap, const std::function<bool(std::uint32_t)>& walkable);

    int width() const { return width_; }
    int height() const { return height_; }
    double cell_size() const { return cell_size_; }
    bool in_bounds(GridCell cell) const;

    void set_walkable(GridCell cell, bool walkable);
    bool is_walkable(GridCell cell) const;
    // Cost of entering a cell; 1 is normal ground, larger values are avoided when possible.
    void set_cost(GridCell cell, double cost);
    double cost(GridCell cell) const;

    void add_obstacle(GridCell cell);
    void remove_obstacle(GridCell cell);
    // Marks every cell overlapped by a world-space rectangle as a dynamic obstacle.
    void add_obstacle_rect(double left, double top, double right, double bottom);
    void clear_obstacles();
    bool has_obstacle(GridCell cell) const;
    // True when a cell is outside the grid, statically unwalkable, or holds a dynamic obstacle.
    bool is_blocked(GridCell cell) const;

    std::optional<GridCell> world_to_cell(double world_x, double world_y) const;
    std::pair<double, double> cell_center(GridCell cell) const;

    bool allow_diagonal = true;

private:
    std::size_t index(GridCell cell) const;

    int width_;
    int height_;
    double cell_size_;
    double origin_x_;
    double origin_y_;
    std::vector<bool> walkable_;
    std::vector<double> costs_;
    std::vector<std::uint16_t> obstacles_;
};

// A* search. Returns cells from start to goal inclusive, or an empty path when unreachable.
// The start cell may be blocked so agents standing on an obstacle can still leave it.
std::vector<GridCell> find_path(const NavigationGrid& grid, GridCell start, GridCell goal);

enum class NavigationStatus { Idle, Moving, Arrived, Unreachable };

// Entity component. Gameplay or AI chooses a destination; NavigationSystem finds and follows the path.
struct NavigationAgent {
    double speed = 120.0;
    double arrival_distance = 2.0;
    NavigationStatus status = NavigationStatus::Idle;
    std::optional<std::pair<double, double>> destination;
    std::vector<std::pair<double, double>> waypoints;
    std::size_t next_waypoint = 0;
    bool repath_requested = false;

    void set_destination(double x, double y) {
        destination = std::make_pair(x, y);
        repath_requested = true;
    }
    void stop() {
        destination.reset();
        waypoints.clear();
        next_waypoint = 0;
        repath_requested = false;
        status = NavigationStatus::Idle;
    }
};

struct NavigationArrivedEvent {
    std::uint64_t entity;
};

struct NavigationFailedEvent {
    std::uint64_t entity;
};

class NavigationSystem final : public UpdateSystem {
public:
    explicit NavigationSystem(const NavigationGrid& grid, EventBus* events = nullptr)
        : grid_(grid), events_(events) {}
    void update(Scene& scene, const InputState& input, double delta_seconds) override;

private:
    bool plan(Entity& entity, NavigationAgent& agent) const;
    bool route_blocked(const NavigationAgent& agent) const;

    const NavigationGrid& grid_;
    EventBus* events_;
};

} // namespace a2e
