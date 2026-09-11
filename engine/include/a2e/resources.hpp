#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <utility>
#include <unordered_map>

namespace a2e {

class ResourceManager {
public:
    template <typename T>
    void register_resource(const std::string& key, std::shared_ptr<T> resource) {
        if (!resource) throw std::invalid_argument("resource cannot be null");
        resources_.insert_or_assign(key, Entry{std::move(resource), std::type_index(typeid(T))});
    }

    template <typename T>
    std::shared_ptr<T> get(const std::string& key) const {
        auto it = resources_.find(key);
        if (it == resources_.end()) return {};
        if (it->second.type != std::type_index(typeid(T))) throw std::bad_cast();
        return std::static_pointer_cast<T>(it->second.value);
    }

    void unload(const std::string& key) { resources_.erase(key); }
    void clear() { resources_.clear(); }
    std::size_t size() const { return resources_.size(); }

private:
    struct Entry {
        std::shared_ptr<void> value;
        std::type_index type;
    };
    std::unordered_map<std::string, Entry> resources_;
};

} // namespace a2e
