// Port of skate-core/src/physics/solver/drive.rs.
#include "s3/core/physics/solver/drive.hpp"

#include "s3/core/physics/solver/joint_data.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::solver::drive {

using namespace joint::data;

namespace {
float symmetric_impulse_limit(float candidate, float limit) {
    const float upper_limited = candidate < limit ? candidate : limit;
    const float negative_limit = 0.0f - limit;
    return upper_limited > negative_limit ? upper_limited : negative_limit;
}
}  // namespace

void solve(Slice<std::uint32_t> record, Slice<std::uint32_t> a, Slice<std::uint32_t> b) {
    const ConstraintGeometry geometry = ConstraintGeometry::decode(record);
    ReactionState body_a = ReactionState::decode(a);
    ReactionState body_b = ReactionState::decode(b);
    const auto [relative_point, relative_angular] = geometry.relative_corrections(body_a, body_b);
    const Column old_linear = read_column(record, 32);
    const Column old_angular = read_column(record, 48);
    const Column linear_response = project_correction(geometry.linear_projection, relative_point, old_linear);
    const Column angular_response =
        project_correction(geometry.angular_projection, relative_angular, old_angular);
    const Column linear_target = read_column(record, 160);
    const Column angular_target = read_column(record, 176);
    const float linear_limits[3] = {read_column(record, 96)[3], read_column(record, 112)[3], linear_target[3]};
    const float angular_limits[3] = {read_column(record, 128)[3], read_column(record, 144)[3],
                                     angular_target[3]};

    Column linear_impulse = old_linear;
    Column angular_impulse = old_angular;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const float linear_candidate = rs::mul_add(linear_response[axis], old_linear[3], linear_target[axis]);
        const float angular_candidate = rs::mul_add(angular_response[axis], old_angular[3], angular_target[axis]);
        linear_impulse[axis] = symmetric_impulse_limit(linear_candidate, linear_limits[axis]);
        angular_impulse[axis] = symmetric_impulse_limit(angular_candidate, angular_limits[axis]);
    }
    Column linear_diff{};
    Column angular_diff{};
    for (std::size_t axis = 0; axis < 4; ++axis) linear_diff[axis] = linear_impulse[axis] - old_linear[axis];
    for (std::size_t axis = 0; axis < 4; ++axis) angular_diff[axis] = angular_impulse[axis] - old_angular[axis];
    const Vector3 linear_change = xyz(linear_diff);
    const Vector3 angular_change = xyz(angular_diff);
    // 82AE2FB8/2FC4 select XYZ from the new impulses while retaining each
    // accumulator's fourth (softness) lane from its old value.
    write_column(record, 32, linear_impulse);
    write_column(record, 48, angular_impulse);
    geometry.apply_impulse_changes(body_a, body_b, linear_change, angular_change);
    publish_reactions(body_a, body_b, a, b);
}

}  // namespace s3::physics::solver::drive
