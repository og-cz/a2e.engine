#pragma once

#include <functional>
#include <cstddef>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace a2e {

class Subscription {
public:
    Subscription() = default;
    explicit Subscription(std::function<void()> cancel) : cancel_(std::move(cancel)) {}

    void unsubscribe() {
        if (cancel_) {
            cancel_();
            cancel_ = {};
        }
    }

private:
    std::function<void()> cancel_;
};

class EventBus {
public:
    template <typename Event>
    Subscription subscribe(std::function<void(const Event&)> listener) {
        const auto type = std::type_index(typeid(Event));
        const auto id = next_id_++;
        listeners_[type].push_back(Entry{
            id, [listener = std::move(listener)](const void* event) { listener(*static_cast<const Event*>(event)); }});
        return Subscription([this, type, id] { unsubscribe(type, id); });
    }

    template <typename Event>
    void publish(const Event& event) const {
        const auto found = listeners_.find(std::type_index(typeid(Event)));
        if (found == listeners_.end()) return;
        for (const auto& entry : found->second) {
            if (entry.listener) entry.listener(&event);
        }
    }

    void clear() { listeners_.clear(); }

private:
    using Listener = std::function<void(const void*)>;
    struct Entry {
        std::size_t id;
        Listener listener;
    };

    void unsubscribe(std::type_index type, std::size_t id) {
        const auto found = listeners_.find(type);
        if (found == listeners_.end()) return;
        for (auto& entry : found->second) {
            if (entry.id == id) {
                entry.listener = {};
                return;
            }
        }
    }

    std::unordered_map<std::type_index, std::vector<Entry>> listeners_;
    std::size_t next_id_ = 1;
};

} // namespace a2e
