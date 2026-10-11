// Port of skate-core/src/physics/skeleton_landing_on_board.rs.
#include "s3/core/physics/skeleton_landing_on_board.hpp"

#include "s3/core/animation/foot_ik.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/player/wipeout_state/math.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::skeleton_landing_on_board {

using animation::foot_ik::interpolate_affine;
using board_motion_output::inverse_length_squared;
using player::wipeout_state::math::cross;
using player::wipeout_state::math::dot;
using player::wipeout_state::math::scale;
using player::wipeout_state::math::sub;
using skeleton_animation_record::compose_affine;
using skeleton_animation_record::transform_point;
using skeleton_root::inverse_rigid;
using skeleton_root::orthonormalize;

namespace {
constexpr Vector UP{0.0f, 1.0f, 0.0f, 0.0f};
constexpr float EPSILON = rs::from_bits(0x37800000u);
}  // namespace

Frame skate_root(Frame old, Vector physical_com, std::uint32_t flags2480, float ground_y, Settings settings) {
    Vector position = physical_com;
    position[1] = old[3][1] + settings.root_y_offset;
    if ((flags2480 & 1u) != 0) {
        const float requested = (ground_y + settings.capsule_length) + settings.capsule_radius;
        position[1] = rs::clamp(requested, position[1], position[1] + 0.3f);
    }
    return Frame{old[2], old[0], old[1], position};
}

void update_root(SkeletonRootFrames& roots, Vector com, Vector local_com, float spin,
                 std::optional<std::uint32_t> revert_frames, bool reversed) {
    roots.initialize_heading = true;
    // `Some(0)` would divide by zero and make every row of the root NaN; see the
    // same guard in `skeleton_air_frames`. A revert with no frames falls back to
    // the ordinary spin rather than poisoning the landing root.
    float angle = spin;
    if (revert_frames && *revert_frames > 0) {
        const float magnitude = rs::f32::consts::PI / static_cast<float>(*revert_frames);
        angle = reversed ? magnitude : -magnitude;
    }
    const auto [sin, cos] = trigonometry::sin_cos(angle);
    const Frame yaw{{
        {cos, 0.0f, -sin, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {sin, 0.0f, cos, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
    Frame frame = compose_affine(roots.animation_to_world, yaw);
    if (revert_frames.has_value()) {
        frame = orthonormalize(frame);
    }
    //82BE0CC0: world-up correction with two independent degeneracy tests.
    const Vector deviation = cross(frame[1], UP);
    if (dot(deviation, deviation) > EPSILON) {
        Vector right = cross(UP, frame[2]);
        const float sq = dot(right, right);
        if (sq > EPSILON) {
            right = scale(right, inverse_length_squared(sq, 2));
            const Frame target{right, UP, cross(right, UP), frame[3]};
            frame = interpolate_affine(frame, target, 0.1f);
        }
    }
    Frame rotation = frame;
    rotation[3] = {0.0f, 0.0f, 0.0f, 0.0f};
    frame[3] = sub(com, transform_point(rotation, local_com));
    roots.reset_initial_alignment(orthonormalize(frame));
}

std::optional<Frame> pose_adjustment(const Frame& world_to_animation, const Frame& actual_board,
                                     const Frame& mapped_board, Vector ik_offset, float com_velocity_y,
                                     std::uint32_t flags2480, float time, const PointGraph<8>& blend) {
    if (!(com_velocity_y < 0.0f) || (flags2480 & 2u) != 0) {
        return std::nullopt;
    }
    Frame target = compose_affine(world_to_animation, actual_board);
    Frame rotation = world_to_animation;
    rotation[3] = {0.0f, 0.0f, 0.0f, 0.0f};
    Vector correction = transform_point(rotation, ik_offset);
    correction[1] = 0.0f;
    if (target[3][1] > mapped_board[3][1]) {
        correction[1] = target[3][1] - mapped_board[3][1];
    }
    for (std::size_t lane = 0; lane < 4; ++lane) target[3][lane] = mapped_board[3][lane] + correction[lane];
    const Frame blended = interpolate_affine(mapped_board, target, blend.evaluate(time));
    Frame offset = compose_affine(blended, inverse_rigid(mapped_board));
    offset[3][1] = -offset[3][1];
    return offset;
}

}  // namespace s3::physics::skeleton_landing_on_board
