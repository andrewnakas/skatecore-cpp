// Port of skate-core/src/player/input_phase/pose_output.rs (whole file).
#include "s3/core/player/input_phase/pose_output.hpp"

#include "s3/core/input/angle.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state/orientation.hpp"

namespace s3::player::input_phase::pose_output {

void SkeletonOutputFields::publish_deck_angles(std::array<float, 4> forward, bool board_flipped) {
    using physics::native_arithmetic::dot3;
    using physics::native_arithmetic::reciprocal_estimate;
    if (board_flipped) {
        forward = rs::map(forward, [](float v) { return -v; });
    }
    const float x = forward[0];
    const float z = forward[2];
    const float reciprocal = reciprocal_estimate(z);
    const float refined = rs::mul_add(reciprocal, rs::mul_add(-reciprocal, z, 1.0f), reciprocal);
    const float basic = input::angle::atan(rs::mul_add(x, refined, 0.0f));
    const std::uint32_t sign = rs::to_bits(x) & 0x80000000u;
    const float pi = rs::from_bits(0x40490fdbu | sign);
    const float half_pi = rs::from_bits(0x3fc90fdbu | sign);
    const float angle = 0.0f > z ? pi + basic : basic;
    // Native equality selection includes +/-zero, even when x is also zero.
    deck_yaw_536 = z == 0.0f ? half_pi : angle;
    deck_pitch_540 = dot3(forward, std::array<float, 4>{0.0f, 1.0f, 0.0f, 0.0f});
}

void SkeletonOutputFields::publish_twist(const physics::skeleton_body::record::SkeletonPhysicalRecord& record,
                                         std::array<float, 4> processed_forward_352,
                                         std::array<float, 4> reckoning_up_1152) {
    using physics::board_motion_output::inverse_length_squared;
    using physics::native_arithmetic::dot3;
    // SkeletonState at6464, physical pose at+1552: translations of
    // parts10/6 are Skeleton8704/8448, respectively.
    const auto across =
        rs::from_fn<4>([&](std::size_t i) { return record.pose[10][3][i] - record.pose[6][3][i]; });
    const float squared = dot3(across, across);
    const float inverse = inverse_length_squared(squared, 2);
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    // Original830BD350 is a zero vector. Preserve the ordered comparison,
    // including the zero/NaN path selecting the zero direction.
    std::array<float, 4> direction{};
    if (length > 0.0f) {
        direction = rs::map(across, [&](float v) { return v * inverse; });
    }
    const float angle =
        wipeout_state::orientation::projected_angle(processed_forward_352, direction, reckoning_up_1152);
    // Original82139A60/50; wrap via fractional turns, not angle modulo pi.
    const float turns = angle * rs::from_bits(0x3E22F983u);
    const float fraction = turns - rs::floor(turns);
    twist_504 = (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40C90FDBu);
}

}  // namespace s3::player::input_phase::pose_output
