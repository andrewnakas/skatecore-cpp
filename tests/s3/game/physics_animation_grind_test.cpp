// Port of skate-game/src/physics/animation_grind_tests.rs (mounted as
// physics::animation_phase::animation_grind::tests).
//! Independent publication tests; these do not simulate physical producers.
#include "harness.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/physics/animation_grind.hpp"

using namespace s3::game::physics::animation_phase::animation_grind;
using s3::animation::skeleton_input::name::encode;
using s3::physics::filtered_state::FilteredCategory;
using s3::physics::filtered_state::GrindState;

S3_TEST(encoded_name_boundary_is_lossless_and_rejects_malformed_names) {
    for (const char* text : {"", "BF_50_50", "FS_BLUNT", "BS_NOSESLIDE", "0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZ"}) {
        const auto name = encode(std::string_view(text));
        CHECK(encode(std::string_view(name_text(name).unwrap())) == name);
    }
    CHECK(name_text(AttributeName{{UINT32_MAX, 0, 0, 0, 0}}).is_err());
    CHECK(name_text(AttributeName{{0, 79235168u, 0, 0, 0}}).is_err());
}

S3_TEST(completed_output_fields_reach_both_graph_boundaries_without_proxies) {
    PhysicalPlayerInput p{};
    p.filtered_state_0 = static_cast<std::uint32_t>(FilteredCategory::Grind);
    p.skateboard.vector_80 = s3::rs::to_bits(std::array<float, 4>{1.0f, 2.0f, 3.0f, 4.0f});
    const std::array<float, 3> board_forward = {5.0f, 6.0f, 7.0f};
    p.ground.flag_273 = 2;
    p.skeleton.twist_504 = -0.7f;
    p.grinds.words_136_140[0] = 4;
    p.grinds.trick_out_240 = 2;
    p.grinds.animation_chromosome_268[0] = 1;
    p.grinds.dropping_in_324 = 3;
    p.air.flag_443 = 5;
    p.air.scalar_184 = 0.15f;
    FilteredStateOutput filtered{};
    filtered.category = FilteredCategory::Grind;
    filtered.previous_category = FilteredCategory::Air;
    filtered.grinding = true;
    filtered.grind = GrindState{};
    filtered.grind.name = encode(std::string_view("BF_50_50"));
    filtered.grind.crouch = 0.3f;
    filtered.last_grind_distance = 0.0f;
    const auto obs = observations(p, &filtered, 0.8f, true, board_forward).unwrap();
    const auto& state = std::get<0>(obs);
    const auto& grind = std::get<1>(obs);
    const auto& conditions = std::get<2>(obs);
    CHECK(state.grinding);
    CHECK(encode(std::string_view(state.grind_name)) == filtered.grind.name);
    CHECK(grind.grind_name == filtered.grind.name);
    CHECK((grind.ground_axis == std::array<float, 3>{1.0f, 2.0f, 3.0f}));
    CHECK((grind.board_axis == std::array<float, 3>{5.0f, 6.0f, 7.0f}));
    CHECK(grind.ground_flag_273 && grind.animation_mirrored);
    CHECK(grind.height == 0.8f && grind.crouch == 0.3f && grind.twist == -0.7f);
    using motion_grind::conditions::Condition;
    for (const Condition condition :
         {Condition(Condition::BluntingBackslash{}), Condition(Condition::Approach{true}),
          Condition(Condition::TrickOutType{2}), Condition(Condition::LandingIntoGrind{}),
          Condition(Condition::DroppingIn{})}) {
        CHECK(condition.evaluate(conditions));
    }
    // No physical state/contact/holding-board proxies were supplied above.
    CHECK_EQ(p.state.state_16, 0u);
    CHECK_EQ(p.grinds.flag_318, 0);
    CHECK_EQ(p.off_board.flag_304, 0);
    CHECK(observations(p, nullptr, 0.8f, true, board_forward).is_err());
    p.filtered_state_0 = static_cast<std::uint32_t>(FilteredCategory::Air);
    CHECK(observations(p, &filtered, 0.8f, true, board_forward).is_err());
}

S3_TEST(initial_reset_is_not_an_invented_grind_publication) {
    const auto obs = observations(PhysicalPlayerInput{}, nullptr, 0.0f, false, {0.0f, 0.0f, 0.0f}).unwrap();
    const auto& state = std::get<0>(obs);
    const auto& grind = std::get<1>(obs);
    CHECK(!state.grinding && !grind.grinding);
    CHECK_EQ(state.category, 0u);
    CHECK_EQ(state.grind_name, std::string(""));
    CHECK_EQ(grind.crouch, 0.0f);
}
