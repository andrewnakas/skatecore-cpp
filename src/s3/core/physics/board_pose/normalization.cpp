// Port of skate-core/src/physics/board_pose/normalization.rs.
#include "s3/core/physics/board_pose/normalization.hpp"

namespace s3::physics::board_pose::normalization {

using namespace arithmetic;

PoseMatrix orthonormalize_rotation(PoseMatrix input) {
    const M original = matrix(input);
    M out = original;
    for (int axis = 2; axis >= 0; --axis) {
        V residual = original[axis];
        for (int other = 2; other >= axis + 1; --other) {
            residual = sub(residual, mul(out[other], splat(dot(out[other], original[axis], false))));
        }
        out[axis] = normalize(residual, false);
    }
    return words(out);
}

PoseMatrix orthonormalize_part_basis(PoseMatrix input) {
    M m = matrix(input);
    const auto squares = rs::from_fn<3>([&](std::size_t i) { return dot(m[i], m[i], false); });
    const auto magnitudes = rs::map(squares, [](float v) {
        const float root = v * rsqrt(v);
        return v == 0.0f ? 0.0f : root;
    });
    for (std::size_t axis = 0; axis < 3; ++axis) {
        m[axis] = mul(m[axis], splat(rsqrt(squares[axis])));
    }
    std::size_t u, v, w;
    auto set = [&](std::size_t a, std::size_t b, std::size_t c) {
        u = a;
        v = b;
        w = c;
    };
    if (!(magnitudes[0] > 0.0f)) {
        set(1, 2, 0);
    } else if (!(magnitudes[1] > 0.0f)) {
        set(2, 0, 1);
    } else if (!(magnitudes[2] > 0.0f)) {
        set(0, 1, 2);
    } else {
        const float ca = rs::abs(dot(m[2], m[0], false));
        const float bc = rs::abs(dot(m[1], m[2], false));
        const float ab = rs::abs(dot(m[0], m[1], false));
        if (ca > bc) {
            if (ab > bc) {
                set(1, 2, 0);
            } else {
                set(0, 1, 2);
            }
        } else if (ab > ca) {
            set(2, 0, 1);
        } else {
            set(0, 1, 2);
        }
    }
    m[w] = normalize(cross(m[u], m[v]), false);
    m[v] = normalize(cross(m[w], m[u]), false);
    return words(m);
}

}  // namespace s3::physics::board_pose::normalization
