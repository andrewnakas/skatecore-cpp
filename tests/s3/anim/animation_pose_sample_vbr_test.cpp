// Port of skate-core/src/animation/tests/pose_sample_vbr.rs.
#include "harness.hpp"
#include "s3/core/animation/pose_sample.hpp"

using namespace s3::animation::pose_sample;

S3_TEST(boundary_uses_second_key_but_retains_two_frame_normalization) {
    Sqt first;
    first.scale = {1.0f, 1.0f, 1.0f, 1.0f};
    first.rotation = {0.0f, 0.0f, 2.0f, 0.0f};
    first.translation = {7.0f, 0.0f, 0.0f, 1.0f};
    Sqt second = first;
    second.rotation = {0.0f, 0.0f, 0.0f, 2.0f};
    second.translation = {8.0f, 0.0f, 0.0f, 1.0f};
    const FrameSelection at_boundary{7, 8, 0.25f};
    const Sqt sampled = sample_key_vbr(first, second, at_boundary);
    CHECK(sampled.translation == second.translation);
    CHECK((sampled.rotation == std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}));
    const FrameSelection exact{8, 8, 0.0f};
    CHECK(sample_key_vbr(second, second, exact).rotation == second.rotation);
    const FrameSelection within{6, 7, 0.25f};
    CHECK(sample_key_vbr(first, second, within) == sample_key(first, second, within));
}
