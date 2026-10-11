// Port of skate-core/src/physics/force_queue.rs.
// Board force delivery: TU3 append 0x82C03EF0 and consumer 0x82C03718.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/point_force.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::force_queue {

using math::Basis3;
using math::Vector3;
using point_force::RetailForceAccumulator;

inline constexpr std::size_t FORCE_CAPACITY = 21;

struct QueuedPointForce {
    /// Native tag is retained for attribution; it does not select a body.
    std::uint32_t tag = 0;
    Vector3 force_world;
    Vector3 point_body;
};
S3_DERIVE_EQ(QueuedPointForce, tag, force_world, point_body)

class BoardForceQueue {
public:
    /// TU3 drops the new force when 21 entries are already present. Repeated
    /// tags remain separate entries, in append order; they are not summed here.
    bool append(QueuedPointForce entry) {
        if (count_ == FORCE_CAPACITY) {
            return false;
        }
        entries_[count_] = entry;
        count_ += 1;
        return true;
    }

    Slice<const QueuedPointForce> entries() const { return Slice<const QueuedPointForce>(entries_.data(), count_); }

    /// Every record acts on the deck. The caller resolves the native collection
    /// force-point Y offset; the native null-collection fallback is zero.
    /// Consumption does not clear the records in Skate 3.
    RetailForceAccumulator apply_to_deck(RetailForceAccumulator accumulator, Basis3 deck_basis, float inverse_mass,
                                         Basis3 world_inverse_inertia, float force_point_y_offset) const {
        for (const auto& entry : entries()) {
            const Vector3 point{
                entry.point_body.x + 0.0f,
                entry.point_body.y + force_point_y_offset,
                entry.point_body.z + 0.0f,
            };
            accumulator = point_force::accumulate_point_force(accumulator, entry.force_world, point, deck_basis,
                                                              inverse_mass, world_inverse_inertia);
        }
        return accumulator;
    }

    /// Separate lifecycle operation, corresponding to the queue reset at
    /// 0x82C02360..0x82C02368 (also reset when the board is repositioned).
    void clear() { count_ = 0; }

private:
    std::array<QueuedPointForce, FORCE_CAPACITY> entries_{};
    std::size_t count_ = 0;
};

/// TU3 0x82C06F58: accumulate alternating body masses, then combine. The
/// grouped binary32 additions differ from an ordinary left-to-right sum.
/// Native input is live-body inverse mass at +124, in definition order.
inline float total_body_mass(Slice<const float> inverse_masses) {
    float even = 0.0f;
    float odd = 0.0f;
    const std::size_t pairs = inverse_masses.size() / 2;
    for (std::size_t p = 0; p < pairs; ++p) {
        even = 1.0f / inverse_masses[2 * p] + even;
        odd = 1.0f / inverse_masses[2 * p + 1] + odd;
    }
    const float paired = odd + even;
    return paired + (inverse_masses.size() % 2 == 1 ? 1.0f / inverse_masses[inverse_masses.size() - 1] : 0.0f);
}

}  // namespace s3::physics::force_queue
