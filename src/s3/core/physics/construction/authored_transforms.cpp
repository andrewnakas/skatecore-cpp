// Port of skate-core/src/physics/construction/authored_transforms.rs.
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::drive_frames::authored {

std::array<RetailAffineTransform, 7> authored_body_transforms(AuthoredTransformInputs input) {
    // fmadds82C0AEE4 and fnmsubs82C0AF60: one rounded multiply/add each.
    const float front_z = rs::mul_add(input.deck_mid_length, 0.5f, input.truck_z_position_front);
    const float back_z = -rs::mul_add(input.deck_mid_length, 0.5f, input.truck_z_position_back);
    const float y = input.truck_y_position;
    const float x = input.wheel_x_distance;
    std::array<RetailAffineTransform, 7> transforms;
    transforms.fill(RetailAffineTransform::IDENTITY);
    const Vector3 translations[6] = {
        Vector3{-x, y, front_z}, Vector3{x, y, front_z},    Vector3{-x, y, back_z},
        Vector3{x, y, back_z},   Vector3{0.0f, y, front_z}, Vector3{0.0f, y, back_z},
    };
    for (std::size_t part = 0; part < 6; ++part) {
        transforms[part].translation = translations[part];
    }
    // Literal pi/2 word at820C69C8. Shared polynomial82473A08 was re-audited
    // against fresh disassembly and constants822F97C0..822F985F for this path.
    const auto [sin, cos] = trigonometry::sin_cos(rs::from_bits(0x3fc90fdbu));
    transforms[5].basis = Basis3{{{{cos, 0.0f, -sin}, {0.0f, 1.0f, 0.0f}, {sin, 0.0f, cos}}}};
    return transforms;
}

std::array<std::array<std::uint32_t, 16>, 7> authored_body_pose_records(AuthoredTransformInputs input) {
    const auto transforms = authored_body_transforms(input);
    std::array<std::array<std::uint32_t, 16>, 7> out{};
    for (std::size_t t = 0; t < 7; ++t) {
        std::array<std::uint32_t, 16> words{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            for (std::size_t component = 0; component < 3; ++component) {
                words[axis * 4 + component] = rs::to_bits(transforms[t].basis.columns[axis][component]);
            }
        }
        words[12] = rs::to_bits(transforms[t].translation.x);
        words[13] = rs::to_bits(transforms[t].translation.y);
        words[14] = rs::to_bits(transforms[t].translation.z);
        out[t] = words;
    }
    return out;
}

}  // namespace s3::physics::drive_frames::authored
