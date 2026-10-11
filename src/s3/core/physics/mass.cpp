// Port of skate-core/src/physics/mass.rs.
#include "s3/core/physics/mass.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::mass {

namespace {

/// Final finite path in TU3 `ComputeMassProperties` (`0x82AE7770`). The Xenon
/// reciprocal estimate is followed by two fused Newton refinements for each
/// principal moment. Inverse mass and the spherical energy term use scalar
/// division in the recovered function.
RetailBodyMassProperties finalize_principal_mass(RetailLocalMassFrame local_mass_frame, float mass,
                                                 Vector3 principal_moments,
                                                 float maximum_angular_velocity,
                                                 float angular_drag) {
    const Vector3 inverse_tensor{
        refined_reciprocal(principal_moments.x),
        refined_reciprocal(principal_moments.y),
        refined_reciprocal(principal_moments.z),
    };
    // S3 82AE7A98/7AAC and S2 82AE9DA0/9DB4 retain the left operand
    // only for ordered LT. Rust min would discard a right-hand NaN.
    const float smallest_xy =
        inverse_tensor.x < inverse_tensor.y ? inverse_tensor.x : inverse_tensor.y;
    const float smallest_inverse = smallest_xy < inverse_tensor.z ? smallest_xy : inverse_tensor.z;
    return RetailBodyMassProperties{
        local_mass_frame,
        RetailInertiaDynamics{
            inverse_tensor,
            1.0f / mass,
            1.0f / smallest_inverse,
            RETAIL_UNBOUNDED_VELOCITY,
            maximum_angular_velocity,
            0.0f,
            angular_drag,
        },
    };
}

/// 82AE78BC..79DC: omit negligible local-frame corrections, otherwise store
/// inverseBodyLTM. The inverse translation starts with Z, then adds Y and X
/// through fused operations, as in the native matrix transpose/inverse path.
RetailLocalMassFrame inverse_mass_frame(RetailLocalMassFrame forward, Vector3 inertia) {
    const Vector3 center = forward.translation;
    const float center_squared = native_arithmetic::dot3({center.x, center.y, center.z, 0.0f},
                                                         {center.x, center.y, center.z, 0.0f});
    const float minimum_offset_squared =
        ((inertia.y + inertia.z) + inertia.x) * rs::from_bits(0x358637BEu);
    // S3 82AE78EC / S2 82AE9BE0: bge after fcmpu means LT is clear,
    // so an unordered comparison also retains the inverse mass frame.
    const bool translated = !(center_squared < minimum_offset_squared);
    bool rotated = false;
    for (std::size_t column = 0; column < 3 && !rotated; ++column) {
        for (std::size_t row = 0; row < 3; ++row) {
            const float identity = column == row ? 1.0f : 0.0f;
            if (rs::abs(forward.basis.columns[column][row] - identity) >
                rs::from_bits(0x3A83126Fu)) {
                rotated = true;
                break;
            }
        }
    }
    if (!translated && !rotated) {
        return RetailLocalMassFrame::IDENTITY;
    }
    const auto columns = rs::from_fn<3>([&](std::size_t column) {
        return rs::from_fn<3>([&](std::size_t row) { return forward.basis.columns[row][column]; });
    });
    const std::array<float, 3> negative_center{0.0f - center.x, 0.0f - center.y, 0.0f - center.z};
    const auto translation = rs::from_fn<3>([&](std::size_t row) {
        return rs::mul_add(
            negative_center[0], columns[0][row],
            rs::mul_add(negative_center[1], columns[1][row], negative_center[2] * columns[2][row]));
    });
    return RetailLocalMassFrame{
        Basis3{columns},
        Vector3{translation[0], translation[1], translation[2]},
    };
}

}  // namespace

float refined_reciprocal(float value) {
    float estimate = native_arithmetic::reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) {
        const float residual = rs::mul_add(-estimate, value, 1.0f);
        estimate = rs::mul_add(estimate, residual, estimate);
    }
    return estimate;
}

std::optional<RetailBodyMassProperties> primitive_mass_properties(PartMassInput input,
                                                                  float maximum_angular_velocity,
                                                                  float angular_drag) {
    const auto primitive = primitive_mass_mod::primitive_mass(input.shape);
    if (!primitive) {
        return std::nullopt;
    }
    const float mass =
        input.requested_mass < rs::f32::MIN_POSITIVE ? primitive->volume : input.requested_mass;
    const Vector3 inertia = primitive->moments_per_unit_mass;
    return finalize_principal_mass(RetailLocalMassFrame::IDENTITY, mass,
                                   Vector3{inertia.x * mass, inertia.y * mass, inertia.z * mass},
                                   maximum_angular_velocity, angular_drag);
}

RetailBodyMassProperties aggregate_mass_properties(MassMoments moments, float requested_mass,
                                                   float maximum_angular_velocity,
                                                   float angular_drag) {
    const AggregateMassProperties properties = moments.principal_properties();
    // 82AE79EC BO4/BI24 skips fallback when CR6.LT is clear, including NaN.
    const float mass = requested_mass < rs::f32::MIN_POSITIVE ? properties.volume : requested_mass;
    const Vector3 inertia = properties.moments_per_unit_mass;
    return finalize_principal_mass(inverse_mass_frame(properties.local_mass_frame, inertia), mass,
                                   Vector3{inertia.x * mass, inertia.y * mass, inertia.z * mass},
                                   maximum_angular_velocity, angular_drag);
}

RetailBodyMassProperties retail_wheel_mass_properties() {
    return wheel_mass_properties(WheelMassSettings::STOCK);
}

RetailBodyMassProperties wheel_mass_properties(WheelMassSettings settings) {
    const auto forward = forward_mass_properties(wheel_mass_input(settings));
    if (!forward) S3_PANIC("the stock wheel is a supported sphere");
    return finalize_principal_mass(RetailLocalMassFrame::IDENTITY, forward->mass,
                                   forward->principal_moments,
                                   RETAIL_WHEEL_MAXIMUM_ANGULAR_VELOCITY, 0.0f);
}

RetailBodyMassProperties retail_truck_mass_properties() {
    return truck_mass_properties(TruckMassSettings::STOCK);
}

RetailBodyMassProperties truck_mass_properties(TruckMassSettings settings) {
    const auto forward = forward_mass_properties(truck_mass_input(settings));
    if (!forward) S3_PANIC("the stock truck is a supported capsule");
    return finalize_principal_mass(RetailLocalMassFrame::IDENTITY, forward->mass,
                                   forward->principal_moments, RETAIL_UNBOUNDED_VELOCITY, 0.0f);
}

RetailBodyMassProperties deck_mass_properties(const DeckGeometry& geometry, float mass,
                                              float angular_drag) {
    RetailBodyMassProperties properties = aggregate_mass_properties(
        geometry.mass_moments(), mass, RETAIL_UNBOUNDED_VELOCITY, angular_drag);
    // 82C0A264..278 overrides the deck inverse-body local transform after
    // calculating its principal inertia. Keep that inertia, reset the frame.
    properties.local_mass_frame = RetailLocalMassFrame::IDENTITY;
    return properties;
}

RetailBodyMassProperties retail_deck_mass_properties() {
    const DeckGeometry geometry = DeckGeometry::new_(DeckGeometrySettings::STOCK);
    // physicsdeck.DeckMass * physics_world.SkateboardMassFactor,
    // DeckAngularDrag * the refined fixed-step Simulation::frequency.
    return deck_mass_properties(geometry, 6.0f,
                                rs::from_bits(0x3EE66666u) * rs::from_bits(0x426FFFFFu));
}

std::array<RetailBodyMassProperties, 7> default_skateboard_mass_properties() {
    return {
        retail_wheel_mass_properties(), retail_wheel_mass_properties(),
        retail_wheel_mass_properties(), retail_wheel_mass_properties(),
        retail_truck_mass_properties(), retail_truck_mass_properties(),
        retail_deck_mass_properties(),
    };
}

}  // namespace s3::physics::mass
