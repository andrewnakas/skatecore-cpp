// Port of skate-core/src/physics/tests/rigid_body.rs (+ the #[cfg(test)] helpers in rigid_body.rs).
#include "harness.hpp"
#include "s3/core/physics/mass.hpp"
#include "s3/core/physics/rigid_body.hpp"

namespace {
using namespace s3::physics::rigid_body;
namespace consts = s3::rs::f32::consts;

const Basis3 IDENTITY{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}};

// #[cfg(test)] helpers from rigid_body.rs.
Vector3 multiply_basis(Basis3 basis, Vector3 vector) {
    return Vector3{
        basis.columns[0][0] * vector.x + basis.columns[1][0] * vector.y + basis.columns[2][0] * vector.z,
        basis.columns[0][1] * vector.x + basis.columns[1][1] * vector.y + basis.columns[2][1] * vector.z,
        basis.columns[0][2] * vector.x + basis.columns[1][2] * vector.y + basis.columns[2][2] * vector.z,
    };
}
Vector3 column(Basis3 basis, std::size_t index) {
    const auto c = basis.columns[index];
    return Vector3{c[0], c[1], c[2]};
}
Vector3 add(Vector3 a, Vector3 b) { return Vector3{a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 scale(Vector3 v, float s) { return Vector3{v.x * s, v.y * s, v.z * s}; }
Vector3 cross(Vector3 a, Vector3 b) {
    return Vector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
[[maybe_unused]] float length_squared(Vector3 v) { return dot(v, v); }
RetailQuaternion normalize_quaternion(RetailQuaternion q) {
    const float inverse = s3::rs::recip(s3::rs::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w));
    return RetailQuaternion{q.x * inverse, q.y * inverse, q.z * inverse, q.w * inverse};
}

void assert_close(float actual, float expected) {
    CHECK_MSG(s3::rs::abs(actual - expected) <= 1.0e-6f, "actual " << actual << ", expected " << expected);
}
void assert_vec_close(Vector3 actual, Vector3 expected) {
    assert_close(actual.x, expected.x);
    assert_close(actual.y, expected.y);
    assert_close(actual.z, expected.z);
}

S3_TEST(force_accumulator_applies_inverse_mass_and_point_torque) {
    const auto output = accumulate_point_force(
        RetailForceAccumulator{Vector3{1.0f, 2.0f, 3.0f}, Vector3::ZERO, 7}, Vector3{0.0f, 12.0f, 0.0f},
        Vector3{0.0f, 0.0f, 2.0f}, IDENTITY, 0.5f, IDENTITY);
    assert_vec_close(output.force_acceleration, Vector3{1.0f, 8.0f, 3.0f});
    assert_vec_close(output.torque_acceleration, Vector3{-24.0f, 0.0f, 0.0f});
    CHECK_EQ(output.cool_down, 0u);
}

S3_TEST(body_step_uses_solver_displacements_without_tangent_projection) {
    const RetailSimulationStep simulation{0.25f, 4.0f, 8, 0.0f, Vector3{0.0f, -9.8f, 0.0f}};
    const auto output = integrate_body_rates(
        RetailBodyRates{
            RetailQuaternion::IDENTITY,
            IDENTITY,
            IDENTITY,
            Vector3{1.0f, 2.0f, 3.0f},
            Vector3{4.0f, 0.0f, 0.0f},
            Vector3{0.0f, 2.0f, 0.0f},
            Vector3{0.0f, -4.0f, 0.0f},
            Vector3::ZERO,
            0.0f,
            0,
        },
        RetailInertiaDynamics{Vector3{1.0f, 1.0f, 1.0f}, 0.5f, 1.0f, 100.0f, 100.0f, 0.0f, 0.0f},
        simulation,
        RetailReactionCorrections{
            Vector3{0.0f, 0.25f, 0.0f},
            Vector3{0.0f, 0.0f, 0.5f},
            Vector3{0.0f, 0.0f, 0.25f},
            Vector3{0.125f, 0.0f, 0.0f},
        });

    assert_vec_close(output.state.position, Vector3{2.0f, 2.0f, 3.5f});
    assert_vec_close(output.state.linear_velocity, Vector3{4.0f, 0.0f, 0.0f});
    assert_vec_close(output.state.angular_velocity, Vector3{0.0f, 2.0f, 1.0f});
    assert_vec_close(output.orientation_displacement, Vector3{0.125f, 0.5f, 0.25f});
    assert_vec_close(output.state.force_acceleration, simulation.gravity_acceleration);
    assert_vec_close(output.state.torque_acceleration, Vector3::ZERO);
    CHECK_EQ(output.state.orientation,
             integrate_orientation(RetailQuaternion::IDENTITY, output.orientation_displacement));
}

S3_TEST(drag_is_frequency_minus_drag_on_frame_displacement) {
    const auto output = integrate_body_rates(
        RetailBodyRates{
            RetailQuaternion::IDENTITY,
            IDENTITY,
            IDENTITY,
            Vector3::ZERO,
            Vector3{8.0f, 0.0f, 0.0f},
            Vector3{0.0f, 6.0f, 0.0f},
            Vector3::ZERO,
            Vector3::ZERO,
            100.0f,
            0,
        },
        RetailInertiaDynamics{Vector3{1.0f, 1.0f, 1.0f}, 1.0f, 1.0f, 100.0f, 100.0f, 2.0f, 3.0f},
        RetailSimulationStep{0.25f, 4.0f, 8, 0.0f, Vector3::ZERO}, RetailReactionCorrections{});
    assert_vec_close(output.state.linear_velocity, Vector3{4.0f, 0.0f, 0.0f});
    assert_vec_close(output.state.angular_velocity, Vector3{0.0f, 1.5f, 0.0f});
}

S3_TEST(speed_caps_and_retail_cool_down_branch_are_separate) {
    RetailBodyRates body{
        RetailQuaternion::IDENTITY,
        IDENTITY,
        IDENTITY,
        Vector3::ZERO,
        Vector3{10.0f, 0.0f, 0.0f},
        Vector3{0.0f, 8.0f, 0.0f},
        Vector3::ZERO,
        Vector3::ZERO,
        200.0f,
        2,
    };
    const RetailInertiaDynamics inertia{Vector3{1.0f, 1.0f, 1.0f}, 0.5f, 2.0f, 3.0f, 2.0f, 0.0f, 0.0f};
    const RetailSimulationStep simulation{0.25f, 4.0f, 4, 20.0f, Vector3::ZERO};
    const auto first = integrate_body_rates(body, inertia, simulation, RetailReactionCorrections{});
    assert_close(first.state.linear_velocity.x, 3.0f);
    assert_close(first.state.angular_velocity.y, 2.0f);
    assert_close(first.state.kinetic_energy, 13.0f);
    CHECK_EQ(first.state.cool_down, 3u);

    body = first.state;
    const auto second = integrate_body_rates(body, inertia, simulation, RetailReactionCorrections{});
    CHECK_EQ(second.state.cool_down, 4u);
}

S3_TEST(quaternion_branch_uses_retail_left_multiplication_and_normalizes) {
    const RetailQuaternion quarter_turn_y{0.0f, consts::FRAC_1_SQRT_2, 0.0f, consts::FRAC_1_SQRT_2};
    const Vector3 angular{0.2f, 0.3f, 0.4f};
    const auto actual = integrate_orientation(quarter_turn_y, angular);

    const Vector3 old_vector{quarter_turn_y.x, quarter_turn_y.y, quarter_turn_y.z};
    const Vector3 left_product_vector = add(scale(angular, quarter_turn_y.w), cross(angular, old_vector));
    const auto expected = normalize_quaternion(RetailQuaternion{
        quarter_turn_y.x + 0.5f * left_product_vector.x,
        quarter_turn_y.y + 0.5f * left_product_vector.y,
        quarter_turn_y.z + 0.5f * left_product_vector.z,
        quarter_turn_y.w - 0.5f * dot(angular, old_vector),
    });
    CHECK_EQ(actual, expected);
    assert_close(actual.x * actual.x + actual.y * actual.y + actual.z * actual.z + actual.w * actual.w, 1.0f);
}

S3_TEST(quaternion_basis_and_world_inverse_inertia_preserve_body_axes) {
    const RetailQuaternion quarter_turn_y{0.0f, consts::FRAC_1_SQRT_2, 0.0f, consts::FRAC_1_SQRT_2};
    const Basis3 basis = basis_from_quaternion(quarter_turn_y);
    assert_vec_close(column(basis, 0), Vector3{0.0f, 0.0f, -1.0f});
    assert_vec_close(column(basis, 1), Vector3{0.0f, 1.0f, 0.0f});
    assert_vec_close(column(basis, 2), Vector3{1.0f, 0.0f, 0.0f});

    const Basis3 world = world_inverse_inertia(basis, Vector3{2.0f, 3.0f, 5.0f});
    assert_vec_close(column(world, 0), Vector3{5.0f, 0.0f, 0.0f});
    assert_vec_close(column(world, 1), Vector3{0.0f, 3.0f, 0.0f});
    assert_vec_close(column(world, 2), Vector3{0.0f, 0.0f, 2.0f});
}

S3_TEST(packed_world_inverse_inertia_uses_retail_lane_order) {
    const Basis3 symmetric{{{{2.0f, 3.0f, 5.0f}, {3.0f, 7.0f, 11.0f}, {5.0f, 11.0f, 13.0f}}}};
    const auto packed = pack_world_inverse_inertia(symmetric);
    CHECK_EQ(packed.full, (Vector3{2.0f, 3.0f, 5.0f}));
    CHECK_EQ(packed.split, (Vector3{13.0f, 7.0f, 11.0f}));
    assert_vec_close(multiply_packed_world_inverse_inertia(packed, Vector3{17.0f, 19.0f, 23.0f}),
                     multiply_basis(symmetric, Vector3{17.0f, 19.0f, 23.0f}));
}

S3_TEST(recovered_deck_mass_frame_is_finite_right_handed_and_orthonormal) {
    const auto frame = s3::physics::mass::retail_deck_mass_properties().local_mass_frame;
    const Vector3 right = column(frame.basis, 0);
    const Vector3 up = column(frame.basis, 1);
    const Vector3 at = column(frame.basis, 2);
    for (float value : {right.x, right.y, right.z, up.x, up.y, up.z, at.x, at.y, at.z,
                        frame.translation.x, frame.translation.y, frame.translation.z}) {
        CHECK(s3::rs::is_finite(value));
    }
    CHECK(s3::rs::abs(length_squared(right) - 1.0f) < 2.0e-5f);
    CHECK(s3::rs::abs(length_squared(up) - 1.0f) < 2.0e-5f);
    CHECK(s3::rs::abs(length_squared(at) - 1.0f) < 2.0e-5f);
    CHECK(s3::rs::abs(dot(right, up)) < 2.0e-5f);
    CHECK(s3::rs::abs(dot(right, at)) < 2.0e-5f);
    CHECK(s3::rs::abs(dot(up, at)) < 2.0e-5f);
    CHECK(dot(cross(right, up), at) > 0.99998f);
}
}  // namespace
