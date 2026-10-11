// Port of skate-core/src/animation/pose_sample.rs.
#include "s3/core/animation/pose_sample.hpp"

#include <algorithm>

#include "s3/core/animation/pose_blend.hpp"

namespace s3::animation::pose_sample {

rs::Result<FrameSelection, std::string_view> select_frames(float time, float fps, std::size_t frames,
                                                           bool blend_frames, float truncation_offset) {
    if (frames == 0 || !rs::is_finite(time) || time < 0.0f || !rs::is_finite(fps) || fps <= 0.0f) {
        return rs::make_err(std::string_view("Invalid stock animation sampling input"));
    }
    const float last = static_cast<float>(frames - 1);
    float position = time * fps;
    position = position - last >= 0.0f ? last : position;
    const float offset = blend_frames ? 0.0f : truncation_offset;
    const std::size_t first = rs::as_usize(position + offset);
    if (first >= frames) {
        return rs::make_err(std::string_view("Stock frame selection exceeds decoded samples"));
    }
    const std::size_t second = blend_frames ? std::min(first + 1, frames - 1) : first;
    //82D20C08 truncates to16bits,82D1E090 expands using literal822F88B8.
    const std::uint16_t fraction = rs::as_int<std::uint16_t>((position - static_cast<float>(first)) * 65535.0f);
    return FrameSelection{first, second, static_cast<float>(fraction) * rs::from_bits(0x37800080u)};
}

Sqt sample_key(Sqt first, Sqt second, FrameSelection selection) {
    // Same-frame extraction does not normalize its decoded quaternion.
    if (selection.first == selection.second) return first;
    return pose_blend::blend_sample(first, second, selection.coefficient);
}

Sqt sample_key_vbr(Sqt first, Sqt second, FrameSelection selection) {
    const bool crosses_block = selection.first / 8 != selection.second / 8;
    const bool copies_second = crosses_block && (selection.first % 8 == 7 || selection.second % 8 == 0);
    return sample_key(copies_second ? second : first, second, selection);
}

}  // namespace s3::animation::pose_sample
