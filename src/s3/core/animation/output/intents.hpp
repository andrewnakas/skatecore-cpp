// Port of skate-core/src/animation/output/intents.rs.
// Fixed intent-map specialization constructed by8258F638, used at full
// MotionGraph+12 (82C0E088) and physics packet+8 (8258FEE8).
// Names are six encoded words; no guessed text hashing or guest addresses.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/animation/compat/input_graph_intents.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::output::intents {

using input::graph_intents::IntentMutation;

inline constexpr std::size_t BUCKET_COUNT = 61;
inline constexpr std::size_t ENTRY_CAPACITY = 64;

/// Rust tuple struct `IntentName(pub [u32; 6])`.
struct IntentName {
    std::array<std::uint32_t, 6> _0{};
};
S3_DERIVE_EQ(IntentName, _0)

struct Intent {
    IntentName name;
    float value = 0.0f;
};
S3_DERIVE_EQ(Intent, name, value)

/// Unit struct `IntentCapacityExceeded`.
struct IntentCapacityExceeded {};
S3_DERIVE_EQ_EMPTY(IntentCapacityExceeded)

/// Native construction takes greatest prime<=65 (61), installs64 nodes, then
/// changes maximum load to10000. Thus every valid state of this specialization
/// stays at61 buckets; generic rehash policy is unreachable before exhaustion.
/// Owned bucket vectors represent chains in traversal order. Allocator node
/// identities/free-list addresses are deliberately outside the domain model.
/// (`IntentMap{}` == Rust `IntentMap::default()`.)
struct IntentMap {
    // Private in Rust.
    std::array<std::vector<Intent>, BUCKET_COUNT> buckets{};
    std::size_t len_ = 0;

    static IntentMap default_() { return IntentMap{}; }

    std::size_t len() const { return len_; }
    bool is_empty() const { return len_ == 0; }

    std::optional<float> get(IntentName name) const;

    ///826C20B0/825A0350: ascending buckets, head-to-tail within each chain.
    /// (Rust returns an iterator; this returns the entries in that order.)
    std::vector<Intent> entries() const;

    ///82472CD0/82472E10/824714D0: wrapping sum of all six words, exact
    /// six-word equality, prepend new entries. Existing values are preserved.
    /// At exhausted capacity native faults; the host rejects the new entry.
    rs::Result<bool, IntentCapacityExceeded> insert(Intent intent);

    /// Set the value at an existing native key, or insert a new key. The
    /// native 82471818 lookup returns a value slot before the caller writes
    /// it, so Set must overwrite an existing entry rather than silently
    /// preserving it as `insert` does.
    rs::Result<bool, IntentCapacityExceeded> set(IntentName name, float value);

    /// Remove one exact six-word key using the same bucket and chain order as
    /// native 82BC1068. Removing an entry leaves the relative order of the
    /// remaining chain untouched.
    bool remove(IntentName name);

    /// Apply the graph host's existing mutation protocol at one decoded key.
    /// `IntentName` is deliberately supplied by the graph decoder: this
    /// module does not invent a string-to-six-word encoding.
    rs::Result<bool, IntentCapacityExceeded> apply(IntentName name, const IntentMutation& mutation);

    ///82BC1B68 clears all chains but preserves bucket configuration.
    void clear();

    ///825936FC..82593740: clear then range-insert, not a structural clone.
    /// Each colliding source chain therefore reverses in the destination.
    /// Canonical graph and packet are distinct owned maps. A caller binding a
    /// map to itself must retain it instead, matching the native identity guard.
    void replace_from(const IntentMap& source);
};

}  // namespace s3::animation::output::intents
