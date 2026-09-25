#include "a2e/navigation.hpp"

#include "a2e/scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <tuple>

namespace a2e {

NavigationGrid::NavigationGrid(int width, int height, double cell_size, double origin_x, double origin_y)
    : width_(width), height_(height), cell_size_(cell_size), origin_x_(origin_x), origin_y_(origin_y) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("navigation grid dimensions must be positive");
    if (cell_size <= 0.0) throw std::invalid_argument("navigation cell size must be positive");
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    walkable_.assign(count, true);
    costs_.assign(count, 1.0);
    obstacles_.assign(count, 0);
}

NavigationGrid NavigationGrid::from_tilemap(const TileMap& tilemap,
                                            const std::function<bool(std::uint32_t)>& walkable) {
    if (!walkable) throw std::invalid_argument("walkable predicate cannot be empty");
    NavigationGrid grid(tilemap.width(), tilemap.height(), tilemap.tile_size());
    for (int y = 0; y < tilemap.height(); ++y) {
        for (int x = 0; x < tilemap.width(); ++x) grid.set_walkable({x, y}, walkable(tilemap.tile(x, y)));
    }
    return grid;
}

bool NavigationGrid::in_bounds(GridCell cell) const {
    return cell.x >= 0 && cell.x < width_ && cell.y >= 0 && cell.y < height_;
}

std::size_t NavigationGrid::index(GridCell cell) const {
    if (!in_bounds(cell)) throw std::out_of_range("navigation cell out of range");
    return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(cell.x);
}

void NavigationGrid::set_walkable(GridCell cell, bool walkable) { walkable_[index(cell)] = walkable; }
bool NavigationGrid::is_walkable(GridCell cell) const { return walkable_[index(cell)]; }

void NavigationGrid::set_cost(GridCell cell, double cost) {
    if (cost < 1.0) throw std::invalid_argument("navigation cost must be at least 1");
    costs_[index(cell)] = cost;
}

double NavigationGrid::cost(GridCell cell) const { return costs_[index(cell)]; }

void NavigationGrid::add_obstacle(GridCell cell) {
    auto& count = obstacles_[index(cell)];
    if (count < std::numeric_limits<std::uint16_t>::max()) ++count;
}

void NavigationGrid::remove_obstacle(GridCell cell) {
    auto& count = obstacles_[index(cell)];
    if (count > 0) --count;
}

void NavigationGrid::add_obstacle_rect(double left, double top, double right, double bottom) {
    const int first_x = std::max(0, static_cast<int>(std::floor((left - origin_x_) / cell_size_)));
    const int first_y = std::max(0, static_cast<int>(std::floor((top - origin_y_) / cell_size_)));
    const int last_x = std::min(width_ - 1, static_cast<int>(std::ceil((right - origin_x_) / cell_size_)) - 1);
    const int last_y = std::min(height_ - 1, static_cast<int>(std::ceil((bottom - origin_y_) / cell_size_)) - 1);
    for (int y = first_y; y <= last_y; ++y) {
        for (int x = first_x; x <= last_x; ++x) add_obstacle({x, y});
    }
}

void NavigationGrid::clear_obstacles() { std::fill(obstacles_.begin(), obstacles_.end(), 0); }

bool NavigationGrid::has_obstacle(GridCell cell) const { return obstacles_[index(cell)] != 0; }

bool NavigationGrid::is_blocked(GridCell cell) const {
    return !in_bounds(cell) || !is_walkable(cell) || has_obstacle(cell);
}

std::optional<GridCell> NavigationGrid::world_to_cell(double world_x, double world_y) const {
    const GridCell cell{static_cast<int>(std::floor((world_x - origin_x_) / cell_size_)),
                        static_cast<int>(std::floor((world_y - origin_y_) / cell_size_))};
    if (!in_bounds(cell)) return std::nullopt;
    return cell;
}

std::pair<double, double> NavigationGrid::cell_center(GridCell cell) const {
    return {origin_x_ + (cell.x + 0.5) * cell_size_, origin_y_ + (cell.y + 0.5) * cell_size_};
}

std::vector<GridCell> find_path(const NavigationGrid& grid, GridCell start, GridCell goal) {
    if (!grid.in_bounds(start) || grid.is_blocked(goal)) return {};
    if (start == goal) return {start};

    constexpr double diagonal_cost = 1.4142135623730951;
    const auto width = static_cast<std::size_t>(grid.width());
    const auto to_index = [width](GridCell cell) {
        return static_cast<std::size_t>(cell.y) * width + static_cast<std::size_t>(cell.x);
    };
    const auto heuristic = [&grid, goal](GridCell cell) {
        const double dx = std::abs(cell.x - goal.x);
        const double dy = std::abs(cell.y - goal.y);
        return grid.allow_diagonal ? (dx + dy) + (diagonal_cost - 2.0) * std::min(dx, dy) : dx + dy;
    };

    const auto count = width * static_cast<std::size_t>(grid.height());
    std::vector<double> best(count, std::numeric_limits<double>::infinity());
    std::vector<std::size_t> parent(count, count);
    std::vector<bool> closed(count, false);

    // Ordered by estimated total, then heuristic, then insertion order, so results are deterministic.
    using Node = std::tuple<double, double, std::uint64_t, int, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    std::uint64_t sequence = 0;
    best[to_index(start)] = 0.0;
    open.emplace(heuristic(start), heuristic(start), sequence++, start.x, start.y);

    static constexpr int offsets[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    while (!open.empty()) {
        const auto [estimate, remaining, order, x, y] = open.top();
        open.pop();
        const GridCell current{x, y};
        const auto current_index = to_index(current);
        if (closed[current_index]) continue;
        closed[current_index] = true;

        if (current == goal) {
            std::vector<GridCell> path;
            for (auto step = current_index; step != count; step = parent[step]) {
                path.push_back({static_cast<int>(step % width), static_cast<int>(step / width)});
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        const int directions = grid.allow_diagonal ? 8 : 4;
        for (int direction = 0; direction < directions; ++direction) {
            const int dx = offsets[direction][0];
            const int dy = offsets[direction][1];
            const GridCell next{x + dx, y + dy};
            if (grid.is_blocked(next)) continue;
            const bool diagonal = dx != 0 && dy != 0;
            // Refuse to cut corners past blocked cells.
            if (diagonal && (grid.is_blocked({x + dx, y}) || grid.is_blocked({x, y + dy}))) continue;
            const auto next_index = to_index(next);
            if (closed[next_index]) continue;
            const double step = (diagonal ? diagonal_cost : 1.0) * grid.cost(next);
            const double candidate = best[current_index] + step;
            if (candidate < best[next_index]) {
                best[next_index] = candidate;
                parent[next_index] = current_index;
                const double h = heuristic(next);
                open.emplace(candidate + h, h, sequence++, next.x, next.y);
            }
        }
    }
    return {};
}

bool NavigationSystem::plan(Entity& entity, NavigationAgent& agent) const {
    agent.repath_requested = false;
    agent.waypoints.clear();
    agent.next_waypoint = 0;
    if (!agent.destination) return false;
    const auto start = grid_.world_to_cell(entity.transform().x, entity.transform().y);
    const auto goal = grid_.world_to_cell(agent.destination->first, agent.destination->second);
    if (!start || !goal) return false;
    const auto cells = find_path(grid_, *start, *goal);
    if (cells.empty()) return false;
    // Skip the cell the agent already occupies and finish on the exact destination point.
    for (std::size_t index = 1; index + 1 < cells.size(); ++index) agent.waypoints.push_back(grid_.cell_center(cells[index]));
    agent.waypoints.push_back(*agent.destination);
    return true;
}

bool NavigationSystem::route_blocked(const NavigationAgent& agent) const {
    for (std::size_t index = agent.next_waypoint; index < agent.waypoints.size(); ++index) {
        const auto& [x, y] = agent.waypoints[index];
        const auto cell = grid_.world_to_cell(x, y);
        if (!cell || grid_.is_blocked(*cell)) return true;
    }
    return false;
}

void NavigationSystem::update(Scene& scene, const InputState&, double delta_seconds) {
    for (const auto& entity : scene.entities()) {
        if (!entity->active()) continue;
        auto* agent = entity->get_component<NavigationAgent>();
        if (!agent) continue;
        auto* body = entity->rigid_body();
        const bool use_velocity = body && body->dynamic;

        if (agent->destination && (agent->repath_requested ||
                                   (agent->status == NavigationStatus::Moving && route_blocked(*agent)))) {
            if (plan(*entity, *agent)) {
                agent->status = NavigationStatus::Moving;
            } else {
                agent->status = NavigationStatus::Unreachable;
                if (use_velocity) body->velocity_x = body->velocity_y = 0.0;
                if (events_) events_->publish(NavigationFailedEvent{entity->id()});
                continue;
            }
        }
        if (agent->status != NavigationStatus::Moving) continue;

        auto& transform = entity->transform();
        double budget = agent->speed * delta_seconds;
        double velocity_x = 0.0;
        double velocity_y = 0.0;
        while (agent->next_waypoint < agent->waypoints.size()) {
            const auto [target_x, target_y] = agent->waypoints[agent->next_waypoint];
            const double dx = target_x - transform.x;
            const double dy = target_y - transform.y;
            const double distance = std::sqrt(dx * dx + dy * dy);
            const bool last = agent->next_waypoint + 1 == agent->waypoints.size();
            // Physics moves velocity-driven agents later in the frame, so allow one frame of travel.
            const double reach = use_velocity ? std::max(agent->arrival_distance, agent->speed * delta_seconds)
                                              : (last ? agent->arrival_distance : 1e-9);
            if (distance <= reach) {
                ++agent->next_waypoint;
                continue;
            }
            if (use_velocity) {
                velocity_x = dx / distance * agent->speed;
                velocity_y = dy / distance * agent->speed;
                break;
            }
            if (distance > budget) {
                transform.x += dx / distance * budget;
                transform.y += dy / distance * budget;
                break;
            }
            transform.x = target_x;
            transform.y = target_y;
            budget -= distance;
            ++agent->next_waypoint;
        }
        if (use_velocity) {
            body->velocity_x = velocity_x;
            body->velocity_y = velocity_y;
        }
        if (agent->next_waypoint >= agent->waypoints.size()) {
            agent->status = NavigationStatus::Arrived;
            agent->destination.reset();
            if (events_) events_->publish(NavigationArrivedEvent{entity->id()});
        }
    }
}

} // namespace a2e
