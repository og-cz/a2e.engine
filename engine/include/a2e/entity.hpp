#pragma once

#include "a2e/physics_components.hpp"
#include "a2e/texture.hpp"
#include "a2e/transform.hpp"

#include <any>
#include <cstdint>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace a2e {

struct Renderable {
    double width = 48.0;
    double height = 48.0;
    std::uint32_t color = 0x0055C2FF;
    int layer = 0;
    bool visible = true;
};

class Entity {
public:
    explicit Entity(std::uint64_t id, std::string name = "Entity")
        : id_(id), name_(std::move(name)) {}

    std::uint64_t id() const { return id_; }
    const std::string& name() const { return name_; }
    bool active() const { return active_; }
    void set_active(bool active) { active_ = active; }
    Transform& transform() { return transform_; }
    const Transform& transform() const { return transform_; }
    Renderable* renderable() { return renderable_ ? &*renderable_ : nullptr; }
    const Renderable* renderable() const { return renderable_ ? &*renderable_ : nullptr; }
    void set_renderable(Renderable renderable) { renderable_ = renderable; }
    Sprite* sprite() { return sprite_ ? &*sprite_ : nullptr; }
    const Sprite* sprite() const { return sprite_ ? &*sprite_ : nullptr; }
    void set_sprite(Sprite sprite) { sprite_ = std::move(sprite); }
    Collider* collider() { return collider_ ? &*collider_ : nullptr; }
    const Collider* collider() const { return collider_ ? &*collider_ : nullptr; }
    void set_collider(Collider collider) { collider_ = collider; }
    RigidBody* rigid_body() { return rigid_body_ ? &*rigid_body_ : nullptr; }
    const RigidBody* rigid_body() const { return rigid_body_ ? &*rigid_body_ : nullptr; }
    void set_rigid_body(RigidBody rigid_body) { rigid_body_ = rigid_body; }

    template <typename Component, typename... Arguments>
    Component& add_component(Arguments&&... arguments) {
        const auto key = std::type_index(typeid(Component));
        auto entry = components_.find(key);
        if (entry == components_.end()) {
            entry = components_.emplace(key, std::any(Component(std::forward<Arguments>(arguments)...))).first;
        } else {
            entry->second = Component(std::forward<Arguments>(arguments)...);
        }
        return *std::any_cast<Component>(&entry->second);
    }

    template <typename Component>
    Component* get_component() {
        const auto entry = components_.find(std::type_index(typeid(Component)));
        return entry == components_.end() ? nullptr : std::any_cast<Component>(&entry->second);
    }

    template <typename Component>
    const Component* get_component() const {
        const auto entry = components_.find(std::type_index(typeid(Component)));
        return entry == components_.end() ? nullptr : std::any_cast<Component>(&entry->second);
    }

    template <typename Component>
    bool remove_component() { return components_.erase(std::type_index(typeid(Component))) != 0; }

private:
    std::uint64_t id_;
    std::string name_;
    bool active_ = true;
    Transform transform_;
    std::optional<Renderable> renderable_;
    std::optional<Sprite> sprite_;
    std::optional<Collider> collider_;
    std::optional<RigidBody> rigid_body_;
    std::unordered_map<std::type_index, std::any> components_;
};

} // namespace a2e
