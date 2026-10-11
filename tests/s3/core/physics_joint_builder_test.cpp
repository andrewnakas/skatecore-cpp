// Port of skate-core/src/physics/tests/joint_builder.rs.
#include "harness.hpp"
#include "s3/core/physics/joint_builder.hpp"

namespace {
using namespace s3::physics::joint_builder;

S3_TEST(output_stride_is_the_observed_three_cache_lines) {
    CHECK_EQ(sizeof(RetailJointJacobian), std::size_t(0x180));
}

S3_TEST(inactive_body_contributes_no_mass_or_inertia) {
    // Synthetic identity bodies replace removed generated constructor output.
    RetailJointBodyInput body;
    body.reaction_guest_address = 0;
    body.state = 0;
    body.orientation = RetailQuaternion::IDENTITY;
    body.center_of_mass = Vector3::ZERO;
    body.basis = Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    body.linear_velocity = Vector3::ZERO;
    body.angular_velocity = Vector3::ZERO;
    body.force_acceleration = Vector3::ZERO;
    body.torque_acceleration = Vector3::ZERO;
    body.inverse_mass = 1.0f;
    body.world_inverse_inertia = RetailPackedWorldInverseInertia{Vector3{1.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 0.0f}};
    RetailJointFramesRaw frames{};
    for (std::size_t index : {3, 11, 19}) frames.words[index] = s3::rs::to_bits(1.0f);
    const auto built = build_retail_joint_jacobian(
        RetailJointBuildInput{RetailJointParametersRaw{}, frames, body, body, 0.25f, 0});
    for (std::size_t i = 84; i < 96; ++i) CHECK_EQ(built.words[i], 0u);
}

JointParameters angular_parameters(std::uint32_t swing_mode, std::uint32_t twist_mode) {
    return JointParameters{Vector3::ZERO, Vector3::ZERO, 0.0f, 0.0f, 0.5f, 0.5f, swing_mode, twist_mode};
}

Basis3 identity_basis() { return Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}}; }

S3_TEST(angular_limit_zero_denominators_retain_native_intervals) {
    const Basis3 basis = identity_basis();
    const auto relative = basis.columns;
    const Rqd rqd = create_rqd(RetailQuaternion::IDENTITY, RetailQuaternion::IDENTITY);
    for (std::uint32_t mode : {1u, 2u}) {
        const auto [axes, low, high] = angular_rows(angular_parameters(mode, 1), basis, basis, relative, rqd);
        CHECK_EQ(axes, basis_columns(basis));
        // Original cone cosine guard, planar zero reciprocal guard and twist
        // zero reciprocal guard retain the initialized unlimited intervals.
        CHECK_EQ(low[0], -tu3::FINITE_INFINITY);
        CHECK_EQ(high[0], tu3::FINITE_INFINITY);
        CHECK_EQ(low[1], -tu3::FINITE_INFINITY);
        CHECK_EQ(high[1], tu3::FINITE_INFINITY);
        if (mode == 2) {
            CHECK_EQ(low[2], 0.0f);
            CHECK_EQ(high[2], 0.0f);
        }
    }
}

S3_TEST(cone_inside_limit_still_publishes_predictive_bound) {
    const Basis3 a = identity_basis();
    const Basis3 b{{{{0.8f, 0.6f, 0.0f}, {-0.6f, 0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    const std::array<std::array<float, 3>, 3> relative{{{0.8f, -0.6f, 0.0f}, {0.6f, 0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    const auto rows = angular_rows(angular_parameters(1, 2), a, b, relative,
                                   create_rqd(RetailQuaternion::IDENTITY, RetailQuaternion::IDENTITY));
    const auto& low = std::get<1>(rows);
    // 82AE4238 writes (0.5-0.8)/0.6 even though the current angle is inside
    // the limit; the velocity bounds can then constrain this tick's motion.
    CHECK(s3::rs::abs(low[1] + 0.5f) < 1.0e-6f);
}

S3_TEST(rqd_rows_are_bilinear_without_unit_quaternion_reconstruction) {
    const RetailQuaternion q{1.0f, 2.0f, 3.0f, 4.0f};
    const Rqd rqd = create_rqd(q, RetailQuaternion::IDENTITY);
    // Original 82AE0DB0's bilinear products reduce to these values when B=1.
    CHECK_EQ(rqd.axes, (std::array<Vector3, 3>{Vector3{4.0f, 3.0f, -2.0f}, Vector3{-3.0f, 4.0f, 1.0f},
                                               Vector3{2.0f, -1.0f, 4.0f}}));
    CHECK_EQ(rqd.relative, (RetailQuaternion{-1.0f, -2.0f, -3.0f, 4.0f}));
}
}  // namespace
