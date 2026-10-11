// Port of skate-core/src/physics/tests/drive_solver.rs.
#include "harness.hpp"
#include "s3/core/physics/drive_solver.hpp"

namespace {
using namespace s3::physics::drive_solver;

const float DT = s3::rs::from_bits(0x3C888889u);

RetailDriveBodyState active_body(std::size_t reaction_index) {
    RetailDriveBodyState b;
    b.reaction_index = reaction_index;
    b.state = ACTIVE_BODY;
    b.orientation = RetailQuaternion::IDENTITY;
    b.basis = s3::math::Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    b.center_of_mass = Vector3::ZERO;
    b.linear_velocity = Vector3::ZERO;
    b.angular_velocity = Vector3::ZERO;
    b.force_acceleration = Vector3{0.0f, -9.8f, 0.0f};
    b.torque_acceleration = Vector3::ZERO;
    b.inverse_mass = 1.0f;
    b.world_inverse_inertia = RetailPackedWorldInverseInertia{Vector3{1.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 0.0f}};
    return b;
}

RetailDriveParams no_drive() { return RetailDriveParams{0.0f, 0.0f, 0.0f, RetailDriveType::NoDrive}; }

RetailDriveFrames frames_with_translation(Vector3 translation) {
    return RetailDriveFrames{
        RetailDriveFrame{RetailQuaternion::IDENTITY, Vector3::ZERO},
        RetailDriveFrame{RetailQuaternion::IDENTITY, translation},
    };
}

void assert_near(float actual, float expected) {
    CHECK_MSG(s3::rs::abs(actual - expected) <= 1.0e-6f, "actual=" << actual << " expected=" << expected);
}

S3_TEST(rate_and_acceleration_terms_use_retail_dt_and_dt_squared) {
    auto frame_b = active_body(1);
    frame_b.linear_velocity = Vector3{1.0f, 0.0f, 0.0f};
    frame_b.force_acceleration = Vector3{1.0f, -9.8f, 0.0f};
    const auto rows = build_drive_rows(
        active_body(0), frame_b, frames_with_translation(Vector3::ZERO),
        RetailDriveDynamics{RetailDriveParams{100.0f, 0.0f, 100000.0f, RetailDriveType::HardDrive}, no_drive()},
        DT);
    assert_near(rows.linear_target_impulse[0], 0.5f * (DT + DT * DT));
}

S3_TEST(off_center_linear_impulse_does_not_change_same_pass_angular_candidate) {
    auto frames = frames_with_translation(Vector3::ZERO);
    frames.body_a.translation = Vector3{0.0f, 1.0f, 0.0f};
    frames.body_b.translation = Vector3{0.0f, 1.0f, 0.0f};
    const RetailDriveParams params{100.0f, 0.0f, 100000.0f, RetailDriveType::HardDrive};
    std::array<RetailDriveRows, 1> rows{
        build_drive_rows(active_body(0), active_body(1), frames, RetailDriveDynamics{params, params}, DT)};
    rows[0].linear_target_impulse = {0.0f, 0.0f, 0.1f};
    rows[0].angular_target_impulse = {0.0f, 0.0f, 0.0f};
    std::array<RetailReactionCorrections, 2> reactions{};
    solve_drive_iteration(rows, reactions);
    // 82AE2ED0 reads the angular difference BEFORE the linear reaction writes
    // at 82AE307C. The former port incorrectly canceled this induced torque.
    CHECK_EQ(rows[0].accumulated_angular_impulse, (std::array<float, 3>{0.0f, 0.0f, 0.0f}));
    CHECK(reactions[0].angular_displacement.x > 0.0f);
    CHECK(reactions[1].angular_displacement.x < 0.0f);
    solve_drive_iteration(rows, reactions);
    CHECK(rows[0].accumulated_angular_impulse[0] < 0.0f);
}

RetailDriveRows moving_drive(RetailDriveParams params) {
    auto b = active_body(1);
    b.center_of_mass = Vector3{2.0f, 0.0f, 0.0f};
    b.linear_velocity = Vector3{2.0f, 0.0f, 0.0f};
    b.force_acceleration.x = 4.0f;
    return build_drive_rows(active_body(0), b, frames_with_translation(Vector3::ZERO),
                            RetailDriveDynamics{params, no_drive()}, 0.5f);
}

S3_TEST(hard_drive_caps_position_before_adding_rate_and_acceleration) {
    // Original 82AE1F78..20D0: position 2 is capped to 1, then weighted by
    // 1/(1+c*dt)=1/2. Rate and acceleration each contribute 1 afterward.
    const auto rows = moving_drive(RetailDriveParams{2.0f, 2.0f, 16.0f, RetailDriveType::HardDrive});
    assert_near(rows.linear_target_impulse[0], 1.25f);
    assert_near(rows.linear_softness, 1.0f);
    assert_near(rows.linear_maximum_impulse[0], 2.0f);
}

S3_TEST(soft_drive_weights_spring_damping_and_acceleration_separately) {
    // Original 82AE1E68..1F30: k*dt^2=2, c*dt=1, denominator=4.
    // Position/rate/acceleration weights are 1/2, 1/4, 3/4 respectively.
    const auto rows = moving_drive(RetailDriveParams{8.0f, 2.0f, 16.0f, RetailDriveType::SoftDrive});
    assert_near(rows.linear_target_impulse[0], 1.0f);
    assert_near(rows.linear_softness, 0.75f);
}

S3_TEST(saturated_quaternion_component_uses_all_frame_b_axes) {
    auto b = active_body(1);
    b.orientation = RetailQuaternion{1.0f, 0.0f, 0.0f, 0.0f};
    b.basis.columns = {{{1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}}};
    const auto rows = build_drive_rows(
        active_body(0), b, frames_with_translation(Vector3::ZERO),
        RetailDriveDynamics{no_drive(), RetailDriveParams{100.0f, 0.0f, 100.0f, RetailDriveType::HardDrive}}, 0.5f);
    // Original 82AE20A0..216C uses one saturated component to select all axes.
    for (std::size_t i = 0; i < 3; ++i) {
        const auto a = rows.angular_axes[i];
        assert_near(a.x, b.basis.columns[i][0]);
        assert_near(a.y, b.basis.columns[i][1]);
        assert_near(a.z, b.basis.columns[i][2]);
    }
    assert_near(rows.angular_target_impulse[0], 1.0f);
    for (float v : rows.angular_target_impulse) CHECK(s3::rs::is_finite(v));
}

S3_TEST(drive_anchor_uses_independent_body_basis) {
    auto a = active_body(0);
    a.basis.columns = {{{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}}};
    auto frames = frames_with_translation(Vector3::ZERO);
    frames.body_a.translation = Vector3{2.0f, 0.0f, 0.0f};
    const auto rows =
        build_drive_rows(a, active_body(1), frames, RetailDriveDynamics{no_drive(), no_drive()}, DT);
    // The body quaternion deliberately remains identity: the source reads
    // the independently stored Body+64/+80/+96 for this point transformation.
    CHECK_EQ(rows.arm_a, (Vector3{0.0f, 0.0f, 2.0f}));
}
}  // namespace
