#include "a2e/scene.hpp"

#include <algorithm>

namespace a2e {

Entity& Scene::create_entity(std::string name) {
    entities_.push_back(std::make_unique<Entity>(next_id_++, std::move(name)));
    return *entities_.back();
}

void Scene::destroy_entity(std::uint64_t id) {
    entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                   [id](const auto& entity) { return entity->id() == id; }),
                    entities_.end());
}

Entity* Scene::find_entity(std::uint64_t id) {
    for (const auto& entity : entities_) if (entity->id() == id) return entity.get();
    return nullptr;
}

const Entity* Scene::find_entity(std::uint64_t id) const {
    for (const auto& entity : entities_) if (entity->id() == id) return entity.get();
    return nullptr;
}

TileMap& Scene::create_tilemap(int width, int height, double tile_size, std::uint32_t empty_color) {
    tilemaps_.push_back(std::make_unique<TileMap>(width, height, tile_size, empty_color));
    return *tilemaps_.back();
}

} // namespace a2e
