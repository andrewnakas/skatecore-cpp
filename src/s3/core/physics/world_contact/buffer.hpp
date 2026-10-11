// Port of skate-core/src/physics/world_contact/buffer.rs.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::buffer {

/// Flush sink: receives `records[..count]` as (pointer, count).
using ContactSink = std::function<void(const ContactRecord*, std::size_t)>;

/// Native TempContactBuffer state (+0/+4/+8/+12/+16, +128 records,
/// +12940/+12941/+12942 flags). Caller supplies the actual constructor settings.
/// Guest allocator/publication pointers are represented by the flush callback.
struct ContactBuffer {
    std::uint32_t count = 0;
    std::uint32_t flushed = 0;
    std::uint32_t capacity = 0;
    std::uint32_t dropped = 0;
    float distance_squared_threshold = 0.0f;
    std::uint8_t allow_flush = 0;
    std::uint8_t deferred_reduction = 0;
    std::uint8_t full = 0;
    std::array<ContactRecord, 50> records{};

    /// Complete 82779D60. A full physical buffer with flushing disabled sets
    /// the full flag but does not increment dropped until a subsequent call.
    std::optional<std::size_t> allocate(const ContactSink& sink);

    /// Complete 82779C18. Tests the most recently allocated record against all
    /// previous records. The native caller, not this function, decrements count.
    bool last_is_duplicate();

    void reduce();

    /// Complete 82779E40; sink is the caller's 8277B428 publication operation.
    /// Flushed counts advance by the reduced count, irrespective of sink result.
    void flush(const ContactSink& sink);
};

}  // namespace s3::physics::world_contact::buffer
