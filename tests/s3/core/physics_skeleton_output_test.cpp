// Port of skate-core/src/physics/skeleton_output/tests.rs.
#include "harness.hpp"
#include "s3/core/physics/skeleton_output.hpp"

namespace {
using namespace s3::physics::skeleton_output;
using s3::physics::skeleton_animation_record::IDENTITY;
using s3::point_graph::PointGraph;
namespace consts = s3::rs::f32::consts;

template <std::size_t N>
std::array<Transform, N> identities() {
    std::array<Transform, N> out;
    out.fill(IDENTITY);
    return out;
}

S3_TEST(solved_volume_is_converted_back_to_bone_and_preserves_unmapped_animation) {
    auto forward = identities<24>();
    forward[1][3][0] = 2.0f;
    std::array<std::optional<std::size_t>, 24> parents{};
    parents[1] = 2;
    const SkeletonOutput output{
        s3::rs::from_fn<24>([](std::size_t i) { return i; }),
        Geometry::new_(parents, forward).unwrap(),
        board::BoneIndices{25, 26, 27, 28, 29, 30},
        board::Settings{1.0f, 1.0f, 1.0f, 1.0f},
    };
    auto physical = identities<24>();
    physical[1][3][0] = 12.0f;
    physical[2][3][0] = 7.0f;
    Transform world_to_anim = IDENTITY;
    world_to_anim[3][0] = -3.0f;
    auto globals = identities<36>();
    auto locals = identities<36>();
    locals[35][3][1] = 9.0f;
    auto bodies = identities<6>();
    bodies[0][3][0] = -1.0f;
    bodies[1][3][0] = 1.0f;
    bodies[2][3][0] = 1.0f;
    bodies[3][3][0] = -1.0f;
    const auto truck_frames = identities<2>();
    output
        .publish(Input{physical, world_to_anim,
                       board::Input{bodies, physical[0], truck_frames, 0.0f, 0.0f, {0.0f, 0.0f}}},
                 globals, locals)
        .unwrap();
    CHECK(s3::rs::abs(globals[1][3][0] - 7.0f) < 1e-6f);
    CHECK(s3::rs::abs(locals[1][3][0] - 3.0f) < 1e-6f);
    CHECK_EQ(locals[35][3][1], 9.0f);
}

S3_TEST(wobble_emits_final_sample_before_expiry_and_selects_real_phase_curves) {
    auto curve = [](float value) {
        return PointGraph<8>{
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
            {value, value, value, value, value, value, value, value},
        };
    };
    const wobble::Settings settings{curve(2.0f), curve(3.0f), curve(4.0f), curve(5.0f), 0.0f};
    auto state = wobble::Wobble::default_();
    CHECK(!state.update(settings).sampled);
    state.trigger(true, true);
    const auto final_sample = state.update(settings);
    CHECK(final_sample.sampled);
    CHECK(!final_sample.remains_active);
    CHECK_EQ(final_sample.tilt, -3.0f);
    CHECK_EQ(final_sample.squish, 5.0f);
    CHECK_EQ(state.time, 0.0f);
    CHECK(!state.update(settings).sampled);
    state.trigger(false, false);
    CHECK_EQ(state.update(settings).tilt, 2.0f);
}

S3_TEST(wobble_rotates_about_deck_forward_then_compresses_along_new_up) {
    Transform board = IDENTITY;
    board[3] = {4.0f, 5.0f, 6.0f, 0.0f};
    wobble::apply(wobble::Output{true, consts::FRAC_PI_2, 0.25f, true}, board);
    CHECK(s3::rs::abs(board[0][0]) < 2e-6f);
    CHECK(s3::rs::abs(board[0][1] - 1.0f) < 2e-6f);
    CHECK(s3::rs::abs(board[1][0] + 1.0f) < 2e-6f);
    CHECK_EQ((std::array<float, 3>{board[2][0], board[2][1], board[2][2]}),
             (std::array<float, 3>{0.0f, 0.0f, 1.0f}));
    CHECK(s3::rs::abs(board[3][0] - 3.75f) < 2e-6f);
    CHECK(s3::rs::abs(board[3][1] - 5.0f) < 2e-6f);
    CHECK_EQ(board[3][2], 6.0f);
}
}  // namespace
