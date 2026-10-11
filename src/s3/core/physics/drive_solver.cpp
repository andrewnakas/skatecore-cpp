// Port of skate-core/src/physics/drive_solver.rs.
#include "s3/core/physics/drive_solver.hpp"

#include "s3/core/physics/constraint_frames.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/solver.hpp"

namespace s3::physics::drive_solver {

void solve_drive_rows(Slice<RetailDriveRows> drives, Slice<RetailReactionCorrections> reactions,
                      std::uint32_t maximum_iterations) {
    solver::solve_constraints({}, {}, drives, reactions, maximum_iterations);
}

void solve_drive_iteration(Slice<RetailDriveRows> drives, Slice<RetailReactionCorrections> reactions) {
    solve_drive_rows(drives, reactions, 1);
}

bool is_active(const RetailDriveBodyState& body) { return (body.state & ACTIVE_BODY) == ACTIVE_BODY; }

std::array<Vector3, 3> basis_columns(const math::Basis3& basis) {
    return rs::map(basis.columns,
                   [](const std::array<float, 3>& column) { return Vector3{column[0], column[1], column[2]}; });
}

std::array<float, 3> project(Vector3 vector, const std::array<Vector3, 3>& axes) {
    return rs::map(axes, [&](const Vector3& axis) { return dot(vector, axis); });
}

Vector3 point_rate(Vector3 linear, Vector3 angular, Vector3 arm) {
    return constraint_frames::point_rate(linear, angular, arm);
}

float dot(Vector3 a, Vector3 b) {
    return native_arithmetic::dot3({a.x, a.y, a.z, 0.0f}, {b.x, b.y, b.z, 0.0f});
}

Vector3 cross(Vector3 a, Vector3 b) { return constraint_frames::cross(a, b); }

}  // namespace s3::physics::drive_solver
