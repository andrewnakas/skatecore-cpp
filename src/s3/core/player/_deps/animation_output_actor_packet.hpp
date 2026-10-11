// TEMP DEP (stage P2): replace with stage A header `s3/core/animation/output/actor_packet.hpp`.
// Only `ExternalPhysicsInput` (the item player/** uses) from
// skate-core/src/animation/output/actor_packet.rs; used while the stage-A header is missing.
#pragma once
#if __has_include("s3/core/animation/output/actor_packet.hpp")
#include "s3/core/animation/output/actor_packet.hpp"
#else
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::animation::output::actor_packet {

struct ExternalPhysicsInput {
    /// Ten complete vectors; word representation preserves non-arithmetic bits.
    std::array<std::array<std::uint32_t, 4>, 10> vectors{};
    std::uint32_t flags = 0;
    /// 82592810 copies 160 bytes and only the high seven flag bits.
    void copy_from(const ExternalPhysicsInput& source) {
        vectors = source.vectors;
        flags = (flags & 0x01ffffffu) | (source.flags & 0xfe000000u);
    }
};
S3_DERIVE_EQ(ExternalPhysicsInput, vectors, flags)

}  // namespace s3::animation::output::actor_packet
#endif
