// Port of skate-core/src/animation/pose_sample.rs.
// TU3 timed frame selection82D20788 and decoded VBR interpolation82D1DE08.
// The data adapter supplies complete decoded keys; no clip substitution occurs.
#pragma once
#include <cstddef>
#include <string_view>

#include "s3/core/animation/output.hpp"

namespace s3::animation::pose_sample {

using output::Sqt;

struct FrameSelection {
    std::size_t first = 0;
    std::size_t second = 0;
    float coefficient = 0.0f;
};
S3_DERIVE_EQ(FrameSelection, first, second, coefficient)

/// Clip evaluation supplies bounded nonnegative seconds. Frame blending uses
/// zero truncation offset; disabled blending uses the FetchSys setting at+8.
/// (Error type is Rust's `&'static str`.)
rs::Result<FrameSelection, std::string_view> select_frames(float time, float fps, std::size_t frames,
                                                           bool blend_frames, float truncation_offset);

Sqt sample_key(Sqt first, Sqt second, FrameSelection selection);

/// TU3 VBR's two-frame extraction selects the second compressed block at an
/// 8-frame boundary, and uses its second frame for both decoded keys.
/// Original raw82D1E0C0..E0 selects the block;82E89BD0..BF4 replaces the first
/// intra-block index. Keep the ORIGINAL selection for interpolation: two-frame
/// extraction still normalizes the quaternion, even when both samples coincide.
Sqt sample_key_vbr(Sqt first, Sqt second, FrameSelection selection);

}  // namespace s3::animation::pose_sample
