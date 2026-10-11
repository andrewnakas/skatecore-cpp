// Port of skate-core/src/player/offboard/surface_frame/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/surface_frame.hpp"
#include "s3/core/player/offboard/surface_frame/math.hpp"

namespace {
using namespace s3::player::offboard::surface_frame;
using namespace s3::player::offboard::surface_frame::math;

SurfaceInput surface() { return SurfaceInput{1, UP, Vector{}, {0.0f, 0.0f, 1.0f, 0.0f}, UP}; }
SpringInput spring() {
    return SpringInput{Vector{}, UP, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f},
                       {0.0f, 0.0f, 1.0f, 0.0f}, 0.0f, 0.0f, false};
}

S3_TEST(missing_contact_preserves_both_normals) {
    State state{};
    const State before = state;
    auto input = surface();
    input.flags = 0;
    input.normal = {1.0f, 0.0f, 0.0f, 7.0f};
    state.update_surface(input);
    CHECK(state == before);
}

S3_TEST(large_normal_jump_updates_source_but_rejects_surface) {
    State state{};
    auto input = surface();
    input.normal = {1.0f, 0.0f, 0.0f, 2.0f};
    state.update_surface(input);
    CHECK_EQ(state.source_normal, input.normal);
    CHECK_EQ(state.surface_normal, UP);
}

S3_TEST(accepted_normal_preserves_w_through_normalization) {
    State state{};
    auto input = surface();
    input.normal = {0.0f, 1.0f, 0.0f, 2.0f};
    state.update_surface(input);
    CHECK_EQ(state.surface_normal, (Vector{0.0f, 1.0f, 0.0f, 1.0f}));
}

S3_TEST(edge_degeneracy_and_angle_antiparallel_return_original) {
    State state{};
    auto input = surface();
    input.flags = 5;
    input.edge_point = input.origin;
    state.update_surface(input);
    CHECK_EQ(state.surface_normal, UP);
    const Vector target{0.0f, -1.0f, 0.0f, 3.0f};
    CHECK_EQ(clamp_angle(target, UP, 0.5f), target);
}

S3_TEST(lean_is_separate_from_spring_and_suppression_decays_it) {
    State state{};
    auto input = spring();
    input.right_delta = 1.0f;
    state.update_spring(input);
    CHECK_EQ(state.spring_normal, UP);
    CHECK_EQ(state.spring_delta, Vector{});
    CHECK(std::fabs(state.lean[0] - 0.035f) < 0.000001f);
    CHECK(state.final_up[0] > 0.0f);
    const auto prior = state.lean;
    input.suppress_lean = true;
    state.update_spring(input);
    CHECK_EQ(state.lean, scale(prior, s3::rs::from_bits(0x3f733333u)));
}

S3_TEST(angle_limit_rotates_reference_toward_target_and_retains_target_magnitude) {
    const auto limited = clamp_angle({2.0f, 0.0f, 0.0f, 0.0f}, UP, s3::rs::from_bits(0x3f490fdbu));
    CHECK(std::fabs(length(limited) - 2.0f) < 0.00001f);
    CHECK(std::fabs(limited[0] - limited[1]) < 0.00001f);
    CHECK(limited[0] > 0.0f);
}
}  // namespace
