// Port of skate-core/src/animation/tests/pose_blend.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/animation/pose_blend.hpp"

using namespace s3::animation::pose_blend;

namespace {
Sqt pose(std::array<float, 4> rotation) {
    Sqt s;
    s.scale = {1.0f, 1.0f, 1.0f, 1.0f};
    s.rotation = rotation;
    s.translation = {0.0f, 0.0f, 0.0f, 0.0f};
    return s;
}
using Arr4 = std::array<float, 4>;
}  // namespace

S3_TEST(bump_multi_blend_accumulates_before_normalizing_without_hemisphere_flip) {
    const Sqt a = pose({0.0f, 0.0f, 0.0f, 1.0f});
    Sqt b = pose({0.6f, 0.0f, 0.0f, -0.8f});
    b.translation = {4.0f, 8.0f, 12.0f, 0.0f};
    const Sqt c = pose({0.0f, 0.6f, 0.0f, 0.8f});
    const std::vector<std::vector<Sqt>> poses = {{a}, {b}, {c}};
    const std::array<float, 3> weights = {0.5f, 0.25f, 0.25f};
    const Sqt output = weighted(poses, weights).unwrap()[0];
    CHECK((output.translation == Arr4{1.0f, 2.0f, 3.0f, 0.0f}));
    const Arr4 raw = {0.15f, 0.15f, 0.0f, 0.5f};
    float sum = -0.0f;
    for (float v : raw) sum += v * v;
    const float norm = s3::rs::sqrt(sum);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(s3::rs::abs(output.rotation[i] - raw[i] / norm) < 1e-6f);
    }
}

S3_TEST(weighted_rejects_invalid_weights_and_rotations) {
    const Sqt identity = pose({0.0f, 0.0f, 0.0f, 1.0f});
    for (float weight : {s3::rs::f32::NAN_, s3::rs::f32::INFINITY_, s3::rs::f32::NEG_INFINITY}) {
        const std::vector<std::vector<Sqt>> poses = {{identity}};
        const std::array<float, 1> weights = {weight};
        const auto result = weighted(poses, weights);
        CHECK(result.is_err());
        CHECK(result.unwrap_err() == PoseBufferError(PoseBufferError::NonFiniteWeight{}));
    }
    const float inf = s3::rs::f32::INFINITY_;
    const float nan = s3::rs::f32::NAN_;
    for (Arr4 rotation : {Arr4{0.0f, 0.0f, 0.0f, 0.0f}, Arr4{inf, inf, inf, inf}, Arr4{nan, nan, nan, nan}}) {
        const std::vector<std::vector<Sqt>> poses = {{pose(rotation)}};
        const std::array<float, 1> weights = {1.0f};
        const auto result = weighted(poses, weights);
        CHECK(result.is_err());
        CHECK(result.unwrap_err() == PoseBufferError(PoseBufferError::InvalidQuaternion{}));
    }
    const std::vector<std::vector<Sqt>> poses = {{identity}};
    const std::array<float, 1> weights = {1.0f};
    CHECK(weighted(poses, weights).is_ok());
}

S3_TEST(blend_retains_native_antipodal_and_zero_dot_decisions) {
    const Sqt identity = pose({0.0f, 0.0f, 0.0f, 1.0f});
    const Sqt antipode = pose({0.0f, 0.0f, 0.0f, -1.0f});
    const Sqt same = blend_sample(identity, antipode, 0.5f);
    CHECK(s3::rs::abs(same.rotation[3] - 1.0f) < 1e-6f);
    const Sqt half_turn = pose({1.0f, 0.0f, 0.0f, 0.0f});
    const Sqt quarter = blend_sample(identity, half_turn, 0.5f);
    CHECK(quarter.rotation[0] < 0.0f);  // Native sign gate is >0, not >=0.
    CHECK(s3::rs::abs(s3::rs::abs(quarter.rotation[0]) - s3::rs::f32::consts::FRAC_1_SQRT_2) < 1e-6f);
    CHECK(s3::rs::abs(quarter.rotation[3] - s3::rs::f32::consts::FRAC_1_SQRT_2) < 1e-6f);
}

S3_TEST(nested_blends_preserve_tree_order_and_per_blend_normalization) {
    const Sqt a = pose({0.0f, 0.0f, 0.0f, 1.0f});
    const Sqt b = pose({0.6f, 0.0f, 0.0f, 0.8f});
    Sqt c = pose({0.0f, 0.6f, 0.0f, 0.8f});
    c.translation = {4.0f, 8.0f, 12.0f, 0.0f};
    const Sqt left = blend_sample(blend_sample(a, b, 0.5f), c, 0.25f);
    const Sqt right = blend_sample(a, blend_sample(b, c, 0.25f), 0.5f);
    CHECK((left.translation == Arr4{1.0f, 2.0f, 3.0f, 0.0f}));
    bool any = false;
    for (std::size_t i = 0; i < 4; ++i) any = any || s3::rs::abs(left.rotation[i] - right.rotation[i]) > 0.01f;
    CHECK(any);
}
