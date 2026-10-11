// Port of skate-core/src/physics/drive_preparation/mod.rs.
// Drive-frame preparation in TU3 Island::Step_Solver2 82763B08.
// The active-drive list is visited before thaw/partition and island-table
// construction. Every island mode runs this loop, regardless of body flags.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::drive_preparation {

/// The two quaternion operations in the same stage. No zero-length or finite
/// fallback is supplied here.
inline void normalize_drive_frames(std::array<std::uint32_t, 16>& frames) {
    for (std::size_t offset : {std::size_t(0), std::size_t(8)}) {
        std::array<float, 4> q{};
        for (std::size_t i = 0; i < 4; ++i) q[i] = rs::from_bits(frames[offset + i]);
        const float d = native_arithmetic::dot4(q, q);
        float e = reciprocal_sqrt::estimate(d);
        for (int k = 0; k < 2; ++k) {
            const float squared = e * e;
            const float half = e * 0.5f;
            e = rs::mul_add(half, rs::mul_add(-d, squared, 1.0f), e);
        }
        for (std::size_t i = 0; i < 4; ++i) {
            frames[offset + i] = rs::to_bits(q[i] * e);
        }
    }
}

/// Normalize each nonnull frame record in native active-list order. Entries
/// are frame-record indices, not drive/body indices; duplicate references are
/// processed repeatedly just as live pointers are in 82763BB4..82763C50.
/// Null frames are skipped. Translations and all their packed lanes survive.
inline void normalize_active_drive_frames(Slice<std::array<std::uint32_t, 16>> frames,
                                          const std::vector<std::optional<std::size_t>>& active_frame_indices) {
    for (const auto& index : active_frame_indices) {
        if (index) normalize_drive_frames(frames[*index]);
    }
}

}  // namespace s3::physics::drive_preparation
