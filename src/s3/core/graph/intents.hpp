// Port of skate-core/src/graph/intents.rs.
//! Canonical ActionGraph/MotionGraph intent identity.
//! TU3 FastString constructor823C3BF8 uses six words of82382B50 encoding.
//! Storage is ours; aliases share one value and the most recent write wins.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/rs.hpp"

namespace s3::graph::intents {

/// Decoded names cannot bypass native case folding, NUL termination or the
/// 36-byte limit on insert, read or removal. Kept separate from five-word
/// animation attribute names and graph time tags.
struct IntentMap {
    std::map<std::array<std::uint32_t, 6>, float> values;

    static IntentMap new_() { return IntentMap{}; }

    std::optional<float> insert(std::string_view name, float value) {
        auto [it, inserted] = values.emplace(animation::playback_parameters::intent_key(name), value);
        if (inserted) return std::nullopt;
        const float old = it->second;
        it->second = value;
        return old;
    }

    /// `Option<&f32>`: nullptr when absent.
    const float* get(std::string_view name) const {
        auto it = values.find(animation::playback_parameters::intent_key(name));
        return it == values.end() ? nullptr : &it->second;
    }

    std::optional<float> remove(std::string_view name) {
        auto it = values.find(animation::playback_parameters::intent_key(name));
        if (it == values.end()) return std::nullopt;
        const float old = it->second;
        values.erase(it);
        return old;
    }

    bool contains_key(std::string_view name) const {
        return values.count(animation::playback_parameters::intent_key(name)) != 0;
    }

    void clear() { values.clear(); }

    std::size_t len() const { return values.size(); }

    bool is_empty() const { return values.empty(); }
};
S3_DERIVE_EQ(IntentMap, values)

}  // namespace s3::graph::intents
