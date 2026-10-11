// Port of skate-core/src/player/offboard/contact_toolkit/prefix.rs (whole file).
// Exact semantic fields of the 184-byte prefix copied by82D31040.
#pragma once
#include "s3/core/player/offboard/contact_toolkit/probes.hpp"

namespace s3::player::offboard::contact_toolkit {

struct ContactPrefix {
    Vector position{};         // 0
    Vector normal{};           // 16
    Frame support_frame{};     // 32..95
    Vector target_position{};  // 96
    Vector target_normal{};    // 112
    Vector edge_position{};    // 128
    Vector edge_normal{};      // 144
    float scalar_160 = 0.0f;
    std::uint32_t kind_164 = 0;
    float distance_168 = 0.0f;
    float distance_172 = 0.0f;
    std::uint32_t flags_176 = 0;
    std::uint32_t support_180 = 0;

    /// 82D2DDD0. Note edge normal144 resets to ZERO, not world up. (Also `Default`.)
    static ContactPrefix reset() {
        return ContactPrefix{ZERO, UP, IDENTITY, ZERO, UP, ZERO, ZERO, 0.0f, 0, 1.0e10f, 1.0e10f, 0, 0};
    }
    static ContactPrefix default_() { return reset(); }
    // pub(super)
    void consume_support(const Input& input, const std::array<air::trajectory::QueryResult, 3>& hits,
                         std::uint32_t candidate_flags);
};
S3_DERIVE_EQ(ContactPrefix, position, normal, support_frame, target_position, target_normal, edge_position,
             edge_normal, scalar_160, kind_164, distance_168, distance_172, flags_176, support_180)

}  // namespace s3::player::offboard::contact_toolkit
