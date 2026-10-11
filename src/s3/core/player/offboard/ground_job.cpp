// Port of skate-core/src/player/offboard/ground_job.rs (whole file).
#include "s3/core/player/offboard/ground_job.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::ground_job {

namespace {
Vector madd(Vector v, float scale, Vector offset) {
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(v[i], scale, offset[i]); });
}
Vector cross(Vector a, Vector b) {
    return Vector{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
Vector normalize(Vector v) {
    const float square = physics::native_arithmetic::dot3(v, v);
    float inverse = physics::reciprocal_sqrt::estimate(square);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
    }
    return rs::map(v, [&](float lane) { return lane * inverse; });
}
}  // namespace

namespace detail {
math::Vector3 xyz(Vector v) { return math::Vector3::new_(v[0], v[1], v[2]); }
ground_query::Frame query_frame(const Frame& f) {
    ground_query::Frame q;
    q.right = xyz(f[0]);
    q.up = xyz(f[1]);
    q.forward = xyz(f[2]);
    q.position = xyz(f[3]);
    return q;
}
Frame native_frame(const ground_query::Frame& f) {
    auto v = [](math::Vector3 x) { return Vector{x.x, x.y, x.z, 0.0f}; };
    return Frame{v(f.right), v(f.up), v(f.forward), v(f.position)};
}
}  // namespace detail

Frame sync_frames(ground_entry::State& state, const ContactPrefix& contact, const controller::GroundResult& result) {
    state.flags_144_to_150[3] = result.sliding;
    state.frame_80 = result.physical_frame;
    if ((contact.flags_176 & 1u) != 0) {
        const Vector delta = rs::from_fn<4>([&](std::size_t i) { return contact.position[i] - state.frame_80[3][i]; });
        const float height = physics::native_arithmetic::dot3(delta, state.frame_80[1]);
        // 82D31ECC ble skips the correction; unordered follows the arithmetic.
        if (!(height <= 0.0f)) {
            state.frame_80[3] = madd(state.frame_80[1], height, state.frame_80[3]);
        }
    }
    Frame animation = result.animation_frame;
    if (state.flags_144_to_150[6]) {
        state.duration_180 -= STEP;
        if (!(state.duration_180 > 0.0f)) {
            state.duration_180 = 0.0f;
            state.flags_144_to_150[6] = false;
        }
        state.angle_172 *= rs::from_bits(0x3f666666u);
        const float angle = rs::mul_add(-state.angular_velocity_176, state.duration_180, state.angle_172);
        const auto sc = trigonometry::sin_cos(angle);
        const float sin = sc.first, cos = sc.second;
        const float a20 = animation[2][0];
        // Geometric host frame: do not publish Rodrigues permutation scratch W.
        const Vector forward =
            madd(Vector{sin, 0.0f, cos, 0.0f}, animation[2][2],
                 madd(Vector{0.0f, 1.0f, 0.0f, 0.0f}, animation[2][1],
                      rs::map(Vector{cos, 0.0f, -sin, 0.0f}, [&](float v) { return v * a20; })));
        // Original has no zero-length replacement on these two normalizations.
        const Vector right = normalize(cross(animation[1], forward));
        animation[0] = right;
        animation[1] = normalize(cross(forward, right));
        animation[2] = forward;
    }
    return animation;
}

}  // namespace s3::player::offboard::ground_job
