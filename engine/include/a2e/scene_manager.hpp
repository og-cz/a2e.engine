#pragma once

#include "a2e/scene.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace a2e {

class SceneManager {
public:
    Scene& create_scene(std::string name) {
        if (name.empty()) throw std::invalid_argument("scene name cannot be empty");
        if (scenes_.find(name) != scenes_.end()) throw std::invalid_argument("scene already exists: " + name);
        auto scene = std::make_unique<Scene>(name);
        Scene& result = *scene;
        scenes_.emplace(std::move(name), std::move(scene));
        if (active_name_.empty()) active_name_ = result.name();
        return result;
    }

    bool set_active(const std::string& name) {
        if (scenes_.find(name) == scenes_.end()) return false;
        active_name_ = name;
        return true;
    }

    bool destroy_scene(const std::string& name) {
        const auto found = scenes_.find(name);
        if (found == scenes_.end()) return false;
        const bool destroying_active = name == active_name_;
        scenes_.erase(found);
        if (destroying_active) {
            active_name_.clear();
            if (!scenes_.empty()) active_name_ = scenes_.begin()->first;
        }
        return true;
    }

    void clear() {
        scenes_.clear();
        active_name_.clear();
    }

    Scene* active() {
        const auto found = scenes_.find(active_name_);
        return found == scenes_.end() ? nullptr : found->second.get();
    }

    const Scene* active() const {
        const auto found = scenes_.find(active_name_);
        return found == scenes_.end() ? nullptr : found->second.get();
    }

    Scene* find(const std::string& name) {
        const auto found = scenes_.find(name);
        return found == scenes_.end() ? nullptr : found->second.get();
    }

    const Scene* find(const std::string& name) const {
        const auto found = scenes_.find(name);
        return found == scenes_.end() ? nullptr : found->second.get();
    }

    const std::string& active_name() const { return active_name_; }
    std::size_t size() const { return scenes_.size(); }

private:
    std::unordered_map<std::string, std::unique_ptr<Scene>> scenes_;
    std::string active_name_;
};

} // namespace a2e
