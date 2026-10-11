// Port of skate-game/src/physics/handplant/rotation.rs
//! CreateMtxFromHeadingAndUp82D61B60 and axis-angle frame blend82BD3150.
#include "s3/game/physics/handplant/rotation.hpp"

#include "s3/core/animation/foot_ik/transforms.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/trigonometry.hpp"
#include "s3/game/physics/footplant/math.hpp"

namespace s3::game::physics::handplant::rotation {

using namespace ::s3::game::physics::footplant::math;
using ::s3::animation::foot_ik::transforms::inverse_rigid;
using ::s3::physics::drive_frames::retail_quaternion_from_basis;
using ::s3::physics::skeleton_animation_record::compose_affine;

std::array<V, 4> frame(V heading, V up) {
    if (rs::abs(dot(heading, up)) > rs::from_bits(0x3f7fff58u)) {
        heading = {1.0f, 0.0f, 0.0f, 0.0f};
        if (rs::abs(dot(heading, up)) > rs::from_bits(0x3f7fff58u)) {
            heading = {0.0f, 0.0f, 1.0f, 0.0f};
        }
    }
    const V right = normalize(cross(up, heading));
    const V forward = normalize(cross(right, up));
    return {right, normalize(cross(forward, right)), forward, V{}};
}

std::array<V, 4> blend(std::array<V, 4> a, std::array<V, 4> b, float weight) {
    if (weight >= 1.0f) {
        return b;
    }
    if (weight <= 0.0f) {
        return a;
    }
    const auto relative = compose_affine(b, inverse_rigid(a));
    ::s3::math::Basis3 basis{};
    for (std::size_t i = 0; i < 3; ++i) basis.columns[i] = {relative[i][0], relative[i][1], relative[i][2]};
    const auto q = retail_quaternion_from_basis(basis);
    const float sign = q.w < 0.0f ? -1.0f : 1.0f;
    const float angle = 2.0f * ::s3::trigonometry::acos(rs::clamp(q.w * sign, -1.0f, 1.0f));
    auto result = a;
    if (angle < rs::from_bits(0x3d0efa35u)) {
        for (std::size_t i = 0; i < 3; ++i) {
            result[i] = normalize(madd(sub(b[i], a[i]), weight, a[i]));
        }
    } else {
        const V axis = normalize({q.x * sign, q.y * sign, q.z * sign, 0.0f});
        const auto [sin, cos] = ::s3::trigonometry::sin_cos(angle * weight);
        for (std::size_t i = 0; i < 3; ++i) {
            result[i] = madd(axis, dot(axis, a[i]) * (1.0f - cos), madd(cross(axis, a[i]), sin, scale(a[i], cos)));
        }
    }
    result[3] = madd(sub(b[3], a[3]), weight, a[3]);
    return result;
}

}  // namespace s3::game::physics::handplant::rotation
