// Port of skate-core/src/physics/solver/joint.rs.
#include "s3/core/physics/solver/joint.hpp"

#include "s3/core/rs.hpp"

namespace s3::physics::solver::joint {

using namespace data;

namespace {
/// Native two-sided limit projection at 82AE2CCC..82AE2D00. These offsets are
/// preconditioned limit errors from the builder, not impulse min/max bounds.
/// An error inside the permitted interval produces zero correction.
float joint_limit_correction(float candidate, float low, float high) {
    const float high_error = high + candidate;
    const float low_error = low + candidate;
    const float negative_correction = high_error < 0.0f ? high_error : 0.0f;
    const float positive_correction = low_error > 0.0f ? low_error : 0.0f;
    return positive_correction + negative_correction;
}
}  // namespace

void solve(Slice<std::uint32_t> record, Slice<std::uint32_t> a, Slice<std::uint32_t> b) {
    const ConstraintGeometry geometry = ConstraintGeometry::decode(record);
    ReactionState body_a = ReactionState::decode(a);
    ReactionState body_b = ReactionState::decode(b);
    const auto [relative_point, relative_angular] = geometry.relative_corrections(body_a, body_b);
    const Column old_linear = read_column(record, 32);
    const Column old_angular = read_column(record, 48);
    const Column linear_candidate = project_correction(geometry.linear_projection, relative_point, old_linear);
    const Column angular_candidate =
        project_correction(geometry.angular_projection, relative_angular, old_angular);

    const Column linear_low = read_column(record, 160);
    const Column linear_high = read_column(record, 176);
    // 82AE2C90..2CAC gathers angular bounds from packed fourth lanes. The
    // fourth output duplicates the Z bound; joint accumulators store all lanes.
    const Column angular_low{read_column(record, 96)[3], read_column(record, 112)[3], linear_low[3],
                             linear_low[3]};
    const Column angular_high{read_column(record, 128)[3], read_column(record, 144)[3], linear_high[3],
                              linear_high[3]};
    Column angular_impulse{};
    Column linear_impulse{};
    for (std::size_t axis = 0; axis < 4; ++axis) {
        angular_impulse[axis] =
            joint_limit_correction(angular_candidate[axis], angular_low[axis], angular_high[axis]);
    }
    for (std::size_t axis = 0; axis < 4; ++axis) {
        linear_impulse[axis] = joint_limit_correction(linear_candidate[axis], linear_low[axis], linear_high[axis]);
    }
    Column angular_diff{};
    Column linear_diff{};
    for (std::size_t axis = 0; axis < 4; ++axis) angular_diff[axis] = angular_impulse[axis] - old_angular[axis];
    for (std::size_t axis = 0; axis < 4; ++axis) linear_diff[axis] = linear_impulse[axis] - old_linear[axis];
    const Vector3 angular_change = xyz(angular_diff);
    const Vector3 linear_change = xyz(linear_diff);

    // The two candidates above used the same incoming body corrections. Native
    // stores angular accumulation first, then linear accumulation, then applies
    // the changes through both bodies' inverse mass and inertia.
    write_column(record, 48, angular_impulse);
    write_column(record, 32, linear_impulse);
    geometry.apply_impulse_changes(body_a, body_b, linear_change, angular_change);
    publish_reactions(body_a, body_b, a, b);
}

}  // namespace s3::physics::solver::joint
