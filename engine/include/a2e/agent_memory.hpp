#pragma once

#include "a2e/perception.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace a2e {

struct MemoryRecord {
    Observation observation;
    double importance = 0.5;
    // Current confidence after decay; starts at the observation's confidence.
    double strength = 1.0;
    bool long_term = false;
};

// Decaying episodic memory. Repeated observations of the same source and kind refresh one record.
class AgentMemory {
public:
    explicit AgentMemory(double decay_per_second = 0.2, double forget_below = 0.05,
                         std::size_t short_term_capacity = 32, double long_term_importance = 0.8);

    void remember(const Observation& observation, double importance = 0.5);
    // Applies decay and forgets weak records. Long-term records decay ten times slower.
    void update(double delta_seconds);
    bool forget(std::uint64_t source, const std::string& kind);
    void clear() { records_.clear(); }

    // Matching records, most recent first.
    std::vector<const MemoryRecord*> recall(const std::function<bool(const MemoryRecord&)>& predicate) const;
    // Most recent record with the tag, from any kind, or nullptr.
    const MemoryRecord* latest(const std::string& tag) const;
    const std::vector<MemoryRecord>& records() const { return records_; }
    std::size_t size() const { return records_.size(); }

private:
    double decay_per_second_;
    double forget_below_;
    std::size_t short_term_capacity_;
    double long_term_importance_;
    std::vector<MemoryRecord> records_;
};

using BeliefValue = std::variant<bool, double, std::string, std::pair<double, double>>;

struct Belief {
    BeliefValue value;
    double confidence = 1.0;
    double time = 0.0;
};

// Named facts an agent holds about the world, each with a confidence. Beliefs can be wrong.
class Beliefs {
public:
    void set(const std::string& key, BeliefValue value, double confidence = 1.0, double time = 0.0);
    // Prevents string literals from silently converting to bool.
    void set(const std::string& key, const char* value, double confidence = 1.0, double time = 0.0) {
        set(key, BeliefValue(std::string(value)), confidence, time);
    }
    const Belief* get(const std::string& key) const;
    template <typename T>
    std::optional<T> value(const std::string& key) const {
        const auto* belief = get(key);
        if (!belief) return std::nullopt;
        if (const auto* typed = std::get_if<T>(&belief->value)) return *typed;
        return std::nullopt;
    }
    bool forget(const std::string& key) { return beliefs_.erase(key) != 0; }
    void clear() { beliefs_.clear(); }
    std::size_t size() const { return beliefs_.size(); }

private:
    std::unordered_map<std::string, Belief> beliefs_;
};

} // namespace a2e
