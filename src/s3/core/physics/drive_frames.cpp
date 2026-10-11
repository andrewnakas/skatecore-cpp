// Port of skate-core/src/physics/drive_frames.rs.
#include "s3/core/physics/drive_frames.hpp"

#include "s3/core/physics/mass.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::drive_frames {

namespace {

float refined_reciprocal_square_root(float value) {
    float estimate = native_arithmetic::reciprocal_square_root_estimate(value);
    for (int i = 0; i < 2; ++i) {
        const float square = estimate * estimate;
        const float half_estimate = estimate * 0.5f;
        const float residual = rs::mul_add(-value, square, 1.0f);
        estimate = rs::mul_add(half_estimate, residual, estimate);
    }
    return estimate;
}

RetailDriveFrames wheel_drive_frame(float z_offset) {
    return RetailDriveFrames{
        RetailDriveFrame{RetailQuaternion::IDENTITY, Vector3::ZERO},
        RetailDriveFrame{RetailQuaternion::IDENTITY, Vector3{0.0f, 0.0f, z_offset}},
    };
}

Vector3 multiply_basis_vector(const Basis3& basis, const Vector3& vector) {
    auto component = [&](std::size_t lane) {
        const float first = vector.x * basis.columns[0][lane];
        const float second = rs::mul_add(vector.y, basis.columns[1][lane], first);
        return rs::mul_add(vector.z, basis.columns[2][lane], second);
    };
    return Vector3{component(0), component(1), component(2)};
}

RetailAffineTransform inverse_affine(RetailAffineTransform transform) {
    const auto columns = transform.basis.columns;
    const Basis3 inverse_basis{{{
        {columns[0][0], columns[1][0], columns[2][0]},
        {columns[0][1], columns[1][1], columns[2][1]},
        {columns[0][2], columns[1][2], columns[2][2]},
    }}};
    const Vector3 translated = multiply_basis_vector(inverse_basis, transform.translation);
    return RetailAffineTransform{
        inverse_basis,
        Vector3{0.0f - translated.x, 0.0f - translated.y, 0.0f - translated.z},
    };
}

Basis3 multiply_basis(const Basis3& left, const Basis3& right) {
    Basis3 out;
    for (std::size_t c = 0; c < 3; ++c) {
        const auto& column = right.columns[c];
        for (std::size_t lane = 0; lane < 3; ++lane) {
            const float first = column[0] * left.columns[0][lane];
            const float second = rs::mul_add(column[1], left.columns[1][lane], first);
            out.columns[c][lane] = rs::mul_add(column[2], left.columns[2][lane], second);
        }
    }
    return out;
}

RetailAffineTransform compose_affine(RetailAffineTransform parent, RetailAffineTransform local) {
    const Vector3 translated = multiply_basis_vector(parent.basis, local.translation);
    return RetailAffineTransform{
        multiply_basis(parent.basis, local.basis),
        Vector3{
            translated.x + parent.translation.x,
            translated.y + parent.translation.y,
            translated.z + parent.translation.z,
        },
    };
}

RetailQuaternion quaternion_candidate(float sum, std::array<float, 4> companion, std::size_t dominant) {
    const float inverse_root = refined_reciprocal_square_root(sum);
    const float half_inverse_root = inverse_root * 0.5f;
    const float half_root = (sum * inverse_root) * 0.5f;
    std::array<float, 4> lanes = rs::map(companion, [&](float value) { return value * half_inverse_root; });
    lanes[dominant] = half_root;
    return RetailQuaternion{lanes[0], lanes[1], lanes[2], lanes[3]};
}

drive_parameters::RetailDriveFrameRaw raw_frame(RetailDriveFrame frame) {
    return drive_parameters::RetailDriveFrameRaw{
        {rs::to_bits(frame.orientation.x), rs::to_bits(frame.orientation.y),
         rs::to_bits(frame.orientation.z), rs::to_bits(frame.orientation.w)},
        {rs::to_bits(frame.translation.x), rs::to_bits(frame.translation.y),
         rs::to_bits(frame.translation.z), 0u},
    };
}

Basis3 rotation_x(float angle) {
    const auto [sin, cos] = trigonometry::sin_cos(angle);
    return Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, cos, sin}, {0.0f, -sin, cos}}}};
}

Basis3 rotation_y(float angle) {
    const auto [sin, cos] = trigonometry::sin_cos(angle);
    return Basis3{{{{cos, 0.0f, -sin}, {0.0f, 1.0f, 0.0f}, {sin, 0.0f, cos}}}};
}

Basis3 transpose_multiply_basis(const Basis3& parent, const Basis3& child) {
    Basis3 out;
    for (std::size_t column = 0; column < 3; ++column) {
        const auto& child_column = child.columns[column];
        for (std::size_t lane = 0; lane < 3; ++lane) {
            const auto& parent_column = parent.columns[lane];
            const float first = child_column[0] * parent_column[0];
            const float second = rs::mul_add(child_column[1], parent_column[1], first);
            out.columns[column][lane] = rs::mul_add(child_column[2], parent_column[2], second);
        }
    }
    return out;
}

}  // namespace

std::array<RetailAffineTransform, 2> default_truck_transforms() {
    return calculate_truck_transforms(RETAIL_DEFAULT_TRUCK_TRANSFORM_INPUTS);
}

std::array<RetailAffineTransform, BODY_COUNT> default_body_transforms() {
    return authored_body_transforms(AuthoredTransformInputs::STOCK);
}

std::array<RetailAffineTransform, BODY_COUNT> default_live_body_transforms() {
    auto transforms = default_body_transforms();
    const auto deck_mass_frame = mass::retail_deck_mass_properties().local_mass_frame;
    transforms[index(BodyId::Deck)] = compose_affine(
        transforms[index(BodyId::Deck)],
        inverse_affine(RetailAffineTransform{deck_mass_frame.basis, deck_mass_frame.translation}));
    return transforms;
}

std::array<RetailQuaternion, BODY_COUNT> default_live_body_orientations() {
    return rs::map(default_live_body_transforms(),
                   [](const RetailAffineTransform& transform) {
                       return retail_quaternion_from_basis(transform.basis);
                   });
}

std::array<RetailDriveFrames, 2> default_truck_drive_frames() {
    const auto base = default_truck_transforms();
    return {
        set_drive_frames_2(RetailAffineTransform::IDENTITY, base[1]),
        set_drive_frames_2(RetailAffineTransform::IDENTITY, base[0]),
    };
}

std::array<RetailDriveFrames, 4> default_wheel_drive_frames() {
    const auto positive = wheel_drive_frame(AuthoredTransformInputs::STOCK.wheel_x_distance);
    const auto negative = wheel_drive_frame(-AuthoredTransformInputs::STOCK.wheel_x_distance);
    return {positive, negative, positive, negative};
}

RetailDriveFrames set_drive_frames_2(RetailAffineTransform parent_world,
                                     RetailAffineTransform child_world) {
    const auto parent_frame = relative_parent_transform(parent_world, child_world);
    const RetailDriveFrame child_frame{RetailQuaternion::IDENTITY, Vector3::ZERO};
    return RetailDriveFrames{
        child_frame,
        RetailDriveFrame{retail_quaternion_from_basis(parent_frame.basis), parent_frame.translation},
    };
}

RetailQuaternion retail_quaternion_from_basis(Basis3 basis) {
    const float m00 = basis.columns[0][0];
    const float m01 = basis.columns[1][0];
    const float m02 = basis.columns[2][0];
    const float m10 = basis.columns[0][1];
    const float m11 = basis.columns[1][1];
    const float m12 = basis.columns[2][1];
    const float m20 = basis.columns[0][2];
    const float m21 = basis.columns[1][2];
    const float m22 = basis.columns[2][2];

    const float trace = (m00 + m11) + m22;
    if (trace > 0.0f) {
        return quaternion_candidate((m00 + m11) + (m22 + 1.0f),
                                    {m21 - m12, m02 - m20, m10 - m01, 0.0f}, 3);
    } else if (m00 > m11 && m00 > m22) {
        return quaternion_candidate((m00 - m11) + ((0.0f - m22) + 1.0f),
                                    {0.0f, m01 + m10, m02 + m20, m21 - m12}, 0);
    } else if (m11 > m22) {
        return quaternion_candidate(((0.0f - m00) + m11) + ((0.0f - m22) + 1.0f),
                                    {m01 + m10, 0.0f, m12 + m21, m02 - m20}, 1);
    }
    return quaternion_candidate(((0.0f - m00) - m11) + (m22 + 1.0f),
                                {m02 + m20, m12 + m21, 0.0f, m10 - m01}, 2);
}

drive_parameters::RetailDriveFramesRaw to_raw(RetailDriveFrames frames) {
    return drive_parameters::RetailDriveFramesRaw{raw_frame(frames.body_a), raw_frame(frames.body_b)};
}

std::array<RetailAffineTransform, 2> calculate_truck_transforms(RetailTruckTransformInputs inputs) {
    constexpr float DEGREES_TO_RADIANS = f32_from_bits(0x3C8EFA35u);
    constexpr float HALF = f32_from_bits(0x3F000000u);
    constexpr float NEGATIVE_HALF_PI = f32_from_bits(0xBFC90FDBu);
    constexpr float POSITIVE_HALF_PI = f32_from_bits(0x3FC90FDBu);

    const float alpha = inputs.truck_rotation_axis_angle_degrees * DEGREES_TO_RADIANS;
    const float front_z = rs::mul_add(inputs.deck_mid_length, HALF, inputs.truck_z_position_front);
    const float back_z = -rs::mul_add(inputs.deck_mid_length, HALF, inputs.truck_z_position_back);

    return {
        RetailAffineTransform{
            multiply_basis(rotation_x(alpha), rotation_y(POSITIVE_HALF_PI)),
            Vector3{0.0f, inputs.truck_y_position, back_z},
        },
        RetailAffineTransform{
            multiply_basis(rotation_x(-alpha), rotation_y(NEGATIVE_HALF_PI)),
            Vector3{0.0f, inputs.truck_y_position, front_z},
        },
    };
}

RetailAffineTransform relative_parent_transform(RetailAffineTransform parent_world,
                                                RetailAffineTransform child_world) {
    return RetailAffineTransform{
        transpose_multiply_basis(parent_world.basis, child_world.basis),
        transpose_relative_translation(parent_world.basis, parent_world.translation,
                                       child_world.translation),
    };
}

Vector3 transpose_relative_translation(Basis3 parent_basis, Vector3 parent_translation,
                                       Vector3 child_translation) {
    const Vector3 neg_parent{
        0.0f - parent_translation.x,
        0.0f - parent_translation.y,
        0.0f - parent_translation.z,
    };
    auto component = [&](const std::array<float, 3>& parent_column) {
        float result = neg_parent.z * parent_column[2];
        result = rs::mul_add(neg_parent.y, parent_column[1], result);
        result = rs::mul_add(neg_parent.x, parent_column[0], result);
        result = rs::mul_add(child_translation.x, parent_column[0], result);
        result = rs::mul_add(child_translation.y, parent_column[1], result);
        return rs::mul_add(child_translation.z, parent_column[2], result);
    };
    return Vector3{
        component(parent_basis.columns[0]),
        component(parent_basis.columns[1]),
        component(parent_basis.columns[2]),
    };
}

}  // namespace s3::physics::drive_frames
