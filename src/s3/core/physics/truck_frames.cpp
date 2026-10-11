// Port of skate-core/src/physics/truck_frames.rs.
#include "s3/core/physics/truck_frames.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::physics::truck_frames {

namespace {
RetailAffineTransform rotate_truck(RetailAffineTransform base, float angle) {
    const auto [sin, cos] = trigonometry::sin_cos(angle);
    const std::array<std::array<float, 3>, 3> rotation{{{1.0f, 0.0f, 0.0f}, {0.0f, cos, sin}, {0.0f, -sin, cos}}};
    math::Basis3 out;
    for (std::size_t c = 0; c < 3; ++c) {
        const auto& column = rotation[c];
        for (std::size_t lane = 0; lane < 3; ++lane) {
            const float first = column[0] * base.basis.columns[0][lane];
            const float second = rs::mul_add(column[1], base.basis.columns[1][lane], first);
            out.columns[c][lane] = rs::mul_add(column[2], base.basis.columns[2][lane], second);
        }
    }
    return RetailAffineTransform{out, base.translation};
}
}  // namespace

std::array<RetailDriveFrames, 2> steering_drive_frames(std::array<RetailAffineTransform, 2> base,
                                                       std::array<float, 2> targets) {
    const auto transforms = steering_truck_transforms(base, targets);
    const auto& front = transforms[0];
    const auto& back = transforms[1];
    const std::array<RetailAffineTransform, 2> transformed{back, front};
    return rs::map(transformed, [](const RetailAffineTransform& target) {
        return drive_frames::set_drive_frames_2(RetailAffineTransform::IDENTITY, target);
    });
}

std::array<RetailAffineTransform, 2> steering_truck_transforms(std::array<RetailAffineTransform, 2> base,
                                                               std::array<float, 2> targets) {
    return {rotate_truck(base[0], -targets[0]), rotate_truck(base[1], targets[1])};
}

}  // namespace s3::physics::truck_frames
