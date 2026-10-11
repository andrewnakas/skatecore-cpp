// Port of skate-core/src/physics/skeleton_output/correction.rs.
#include "s3/core/physics/skeleton_output/correction.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::skeleton_output::correction {

using native_arithmetic::dot3;
using native_arithmetic::vector_min;

void apply(SkeletonBody& body, bool& pending, std::array<float, 4> error,
           std::array<float, 4> ground_normal, bool wipeout, std::uint32_t flags_2468,
           std::uint32_t flags_2476) {
    if (wipeout && (flags_2476 & (1u << 30)) != 0) {
        // 82BE2AB0 moves live parts0..23 only when correction length²<1.
        if (1.0f > dot3(error, error)) {
            for (std::size_t part = 0; part < 24; ++part) {
                auto frame = body.record.pose[part];
                for (std::size_t lane = 0; lane < 4; ++lane) {
                    frame[3][lane] += error[lane];
                }
                body.set_part_transform(part, frame);
            }
        }
        pending = false;
    } else if (!wipeout && (flags_2468 & (1u << 18)) == 0 && pending) {
        const float distance = dot3(error, ground_normal);
        const float allowed = vector_min(0.0f, distance);
        std::array<float, 4> offset{};
        for (std::size_t i = 0; i < 4; ++i) {
            const float tangent = error[i] - ground_normal[i] * distance;
            offset[i] = rs::mul_add(ground_normal[i], allowed, tangent);
        }
        // 82BEC2C0 starts at record part1 and translates through23. It leaves
        // the observed board, solver states, COM and velocity history alone.
        for (std::size_t part = 1; part < 24; ++part) {
            for (std::size_t lane = 0; lane < 4; ++lane) {
                body.record.pose[part][3][lane] += offset[lane];
            }
        }
        pending = false;
    }
}

}  // namespace s3::physics::skeleton_output::correction
