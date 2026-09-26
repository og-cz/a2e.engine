#include "a2e/agent_memory.hpp"

#include <algorithm>
#include <stdexcept>

namespace a2e {

AgentMemory::AgentMemory(double decay_per_second, double forget_below, std::size_t short_term_capacity,
                         double long_term_importance)
    : decay_per_second_(decay_per_second), forget_below_(forget_below),
      short_term_capacity_(short_term_capacity), long_term_importance_(long_term_importance) {
    if (decay_per_second < 0.0) throw std::invalid_argument("memory decay cannot be negative");
    if (forget_below < 0.0) throw std::invalid_argument("forget threshold cannot be negative");
    if (short_term_capacity == 0) throw std::invalid_argument("short-term capacity must be positive");
}

void AgentMemory::remember(const Observation& observation, double importance) {
    importance = std::clamp(importance, 0.0, 1.0);
    const double confidence = std::clamp(observation.confidence, 0.0, 1.0);
    if (observation.source != 0) {
        for (auto& record : records_) {
            if (record.observation.source == observation.source && record.observation.kind == observation.kind) {
                record.observation = observation;
                record.importance = std::max(record.importance, importance);
                record.strength = std::max(record.strength, confidence);
                record.long_term = record.long_term || record.importance >= long_term_importance_;
                return;
            }
        }
    }
    records_.push_back({observation, importance, confidence, importance >= long_term_importance_});

    std::size_t short_term = 0;
    for (const auto& record : records_) short_term += record.long_term ? 0 : 1;
    if (short_term <= short_term_capacity_) return;
    // Over capacity: drop the weakest short-term record.
    auto weakest = records_.end();
    for (auto record = records_.begin(); record != records_.end(); ++record) {
        if (record->long_term) continue;
        if (weakest == records_.end() || record->strength < weakest->strength) weakest = record;
    }
    if (weakest != records_.end()) records_.erase(weakest);
}

void AgentMemory::update(double delta_seconds) {
    for (auto& record : records_) {
        const double rate = record.long_term ? decay_per_second_ * 0.1 : decay_per_second_;
        record.strength -= rate * delta_seconds;
    }
    records_.erase(std::remove_if(records_.begin(), records_.end(),
                                  [this](const MemoryRecord& record) { return record.strength < forget_below_; }),
                   records_.end());
}

bool AgentMemory::forget(std::uint64_t source, const std::string& kind) {
    const auto before = records_.size();
    records_.erase(std::remove_if(records_.begin(), records_.end(),
                                  [source, &kind](const MemoryRecord& record) {
                                      return record.observation.source == source && record.observation.kind == kind;
                                  }),
                   records_.end());
    return records_.size() != before;
}

std::vector<const MemoryRecord*> AgentMemory::recall(
    const std::function<bool(const MemoryRecord&)>& predicate) const {
    std::vector<const MemoryRecord*> matches;
    for (const auto& record : records_) {
        if (!predicate || predicate(record)) matches.push_back(&record);
    }
    std::stable_sort(matches.begin(), matches.end(), [](const MemoryRecord* left, const MemoryRecord* right) {
        return left->observation.time > right->observation.time;
    });
    return matches;
}

const MemoryRecord* AgentMemory::latest(const std::string& tag) const {
    const auto matches = recall([&tag](const MemoryRecord& record) { return record.observation.tag == tag; });
    return matches.empty() ? nullptr : matches.front();
}

void Beliefs::set(const std::string& key, BeliefValue value, double confidence, double time) {
    if (key.empty()) throw std::invalid_argument("belief key cannot be empty");
    beliefs_.insert_or_assign(key, Belief{std::move(value), std::clamp(confidence, 0.0, 1.0), time});
}

const Belief* Beliefs::get(const std::string& key) const {
    const auto found = beliefs_.find(key);
    return found == beliefs_.end() ? nullptr : &found->second;
}

} // namespace a2e
