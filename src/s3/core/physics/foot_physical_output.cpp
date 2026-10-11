// Port of skate-core/src/physics/foot_physical_output.rs.
#include "s3/core/physics/foot_physical_output.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::foot_physical_output {

using native_arithmetic::reciprocal_estimate;
using Transform = skeleton_animation_record::AnimationPartTransform;

namespace {

Transform inverse_physical_board(const Transform& board) {
    Transform inverse{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t lane = 0; lane < 3; ++lane) {
            inverse[axis][lane] = board[lane][axis];
        }
    }
    //82BF2398..241C builds -P.z * inverseZ, then Y and X in source order.
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float z = -board[3][2] * inverse[2][lane];
        const float yz = rs::mul_add(-board[3][1], inverse[1][lane], z);
        inverse[3][lane] = rs::mul_add(-board[3][0], inverse[0][lane], yz);
    }
    return inverse;
}

std::array<float, 4> transform_point(const Transform& frame, const std::array<float, 4>& p) {
    std::array<float, 4> out{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float x = rs::mul_add(frame[0][lane], p[0], frame[3][lane]);
        const float xy = rs::mul_add(frame[1][lane], p[1], x);
        out[lane] = rs::mul_add(frame[2][lane], p[2], xy);
    }
    return out;
}

}  // namespace

void FootPhysicalState::reset() { *this = FootPhysicalState{}; }

FootPhysicalOutput FootPhysicalState::update(const SkeletonPhysicalRecord& record, float dt,
                                             FootPhysicalSettings settings) {
    const Transform inverse = inverse_physical_board(record.pose[0]);
    const std::array<std::array<float, 4>, 2> positions{
        transform_point(inverse, record.pose[15][3]),
        transform_point(inverse, record.pose[19][3]),
    };
    float inverse_dt = reciprocal_estimate(dt);
    for (int i = 0; i < 2; ++i) {
        inverse_dt = rs::mul_add(inverse_dt, rs::mul_add(-inverse_dt, dt, 1.0f), inverse_dt);
    }
    FootPhysicalOutput out;
    for (std::size_t foot = 0; foot < 2; ++foot) {
        for (std::size_t lane = 0; lane < 4; ++lane) {
            out.local_velocity[foot][lane] = (positions[foot][lane] - previous_local_toes[foot][lane]) * inverse_dt;
        }
    }
    previous_local_toes = positions;
    //82165A10 is zero: only authored padding supplies the box's Y extent.
    const std::array<float, 4> dimensions{settings.deck_half_width, 0.0f, settings.deck_total_half_length, 0.0f};
    std::array<float, 4> bounds{};
    for (std::size_t lane = 0; lane < 4; ++lane) bounds[lane] = dimensions[lane] + settings.padding[lane];
    out.world_velocity = {record.velocities[15], record.velocities[19]};
    for (std::size_t foot = 0; foot < 2; ++foot) {
        const auto& p = positions[foot];
        out.within_deck_box[foot] = rs::abs(p[0]) < bounds[0] && rs::abs(p[1]) < bounds[1] && rs::abs(p[2]) < bounds[2];
    }
    return out;
}

}  // namespace s3::physics::foot_physical_output
