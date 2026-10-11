// Port of skate-core/tests/animation_output_bounds.rs (crate integration test).
// Independent buffer-validation checks; no generated-code fixtures.
#include "harness.hpp"
#include "s3/core/animation/output.hpp"

using namespace s3::animation::output;

S3_TEST(invalid_buffers_fail_without_partial_publication) {
    const float s = s3::rs::from_bits(0x3f123456u);
    const NativeMatrix sentinel{{{s, s, s, s}, {s, s, s, s}, {s, s, s, s}, {s, s, s, s}}};
    Sqt sqt;
    sqt.scale = {1.0f, 1.0f, 1.0f, 1.0f};
    sqt.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    sqt.translation = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<NativeMatrix, 4> destination = {sentinel, sentinel, sentinel, sentinel};
    const std::array<NativeMatrix, 4> all_sentinel = destination;
    const std::array<Sqt, 1> source = {sqt};
    const auto r1 = sqt_to_local(1, 0, source, destination);
    CHECK(r1.is_err() && r1.unwrap_err() == PoseBufferError(PoseBufferError::ShortInput{}));
    CHECK(destination == all_sentinel);
    const std::array<std::int32_t, 4> parents = {-1, 0, 1, 9};
    const auto r2 = compose_hierarchy(4, parents, -1, all_sentinel, destination);
    CHECK(r2.is_err() && r2.unwrap_err() == PoseBufferError(PoseBufferError::InvalidParent{3, 9}));
    CHECK(destination == all_sentinel);
}
