#pragma once

#include "a2e/entity.hpp"
#include "a2e/tilemap.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace a2e {

class Scene {
public:
    explicit Scene(std::string name = "Scene") : name_(std::move(name)) {}

    Entity& create_entity(std::string name = "Entity");
    void destroy_entity(std::uint64_t id);
    Entity* find_entity(std::uint64_t id);
    const Entity* find_entity(std::uint64_t id) const;
    TileMap& create_tilemap(int width, int height, double tile_size, std::uint32_t empty_color = 0);
    const std::string& name() const { return name_; }
    const std::vector<std::unique_ptr<Entity>>& entities() const { return entities_; }
    const std::vector<std::unique_ptr<TileMap>>& tilemaps() const { return tilemaps_; }

private:
    std::string name_;
    std::uint64_t next_id_ = 1;
    std::vector<std::unique_ptr<Entity>> entities_;
    std::vector<std::unique_ptr<TileMap>> tilemaps_;
};

} // namespace a2e
