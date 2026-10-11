// Port of skate-game/src/graph_host/motion_offboard/match_air_time/tests.rs (module `motion::match_air_time`).
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "assets.hpp"
#include "harness.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/game/animation_pose.hpp"
#include "s3/game/graph_host/motion/match_air_time.hpp"
#include "s3/game/graph_runtime/disk.hpp"

using namespace s3::game::graph_host::motion::match_air_time;
using s3::animation::output::Sqt;
using s3::animation::playback::PlaybackRequest;
using s3::animation::playback::TransitionSettings;
using s3::animation::playback_tree::Evaluation;
using s3::animation::playback_tree::PoseCommand;
using s3::animation::skeleton_input::name::encode;
namespace rs = s3::rs;

S3_TEST(match_air_time_first_update_and_reentry_skip_seek_then_use_remaining_over_total) {
    State state;
    state.begin(0.4f, 0.8f, {0.0f, 0.0f, 0.0f, 0.0f});
    CHECK(state.seek_fraction(0.4f, 0.8f) == std::nullopt);
    state.first_update = false;
    CHECK(state.seek_fraction(0.4f, 0.8f) == std::optional<float>(0.5f));
    CHECK(state.seek_fraction(1.0f, 0.8f) == std::optional<float>(0.0f));
    CHECK(state.seek_fraction(-0.1f, 0.8f) == std::optional<float>(1.0f));
    CHECK(state.seek_fraction(0.0f, 0.0f) == std::nullopt);
    state.begin(0.8f, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f});
    CHECK(state.seek_fraction(0.4f, 1.0f) == std::nullopt);
}

S3_TEST(match_air_time_bounds_displacement_by_xyz_length_and_scales_all_lanes) {
    CHECK(bounded_translation({0.0f, 0.0f, 0.0f, 0.0f}) == (std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}));
    CHECK(bounded_translation({0.001f, 0.0f, 0.0f, 7.0f}) == (std::array<float, 4>{0.001f, 0.0f, 0.0f, 7.0f}));
    const std::array<float, 4> bounded = bounded_translation({0.0f, 3.0f, 4.0f, 10.0f});
    const std::array<float, 4> expected{0.0f, 1.2f, 1.6f, 4.0f};
    for (std::size_t i = 0; i < 4; ++i) CHECK(std::fabs(bounded[i] - expected[i]) < 0.00001f);
}

S3_TEST(match_air_time_publishes_five_selection_parameters_from_correct_owners) {
    auto metadata = s3::data::AnimationMetadata::parse(R"({
        "version":1,"source_bank":"synthetic-test",
        "source_sha256":"0000000000000000000000000000000000000000000000000000000000000000",
        "source_bytes":48,"clips":[],"unsupported_trees":[]
    })");
    if (!metadata.ok) s3test::fail(__FILE__, __LINE__, metadata.error);
    MotionAnimation animation = MotionAnimation::from_metadata(std::move(metadata.value));
    State state;
    state.begin(0.3f, 0.7f, {1.0f, 1.0f, 1.0f, 1.0f});
    state.update(animation, 0.6f, 0.9f, {0.0f, 0.5f, 1.0f, 0.0f});
    const std::pair<const char*, float> cases[] = {{"CadenceStartPercent", 0.3f},
                                                   {"AnimTime", 0.9f},
                                                   {"AnimTransX", 0.0f},
                                                   {"AnimTransY", 0.5f},
                                                   {"AnimTransZ", 1.0f}};
    for (const auto& [name, expected] : cases) {
        CHECK(std::fabs(rs::unwrap(animation.pending_parameter(encode(name))) - expected) < 0.00001f);
    }
    CHECK(!state.first_update);
}

namespace {
bool finite(const std::vector<Sqt>& pose) {
    for (const Sqt& s : pose) {
        for (float v : s.rotation) {
            if (!rs::is_finite(v)) return false;
        }
        for (float v : s.translation) {
            if (!rs::is_finite(v)) return false;
        }
    }
    return true;
}
}  // namespace

// #[ignore = "requires private stock banks; selection/timeline/landing pose bridge, not physical acceptance"]
S3_TEST(stock_match_air_time_selects_jump_advances_pose_and_preserves_landing_identity) {
    S3_REQUIRE_ASSETS(root);
    const s3::data::Vfs vfs = s3::game::graph_runtime::disk::read_banks(root);
    auto banks = s3::data::AnimationBanks::load(vfs, root);
    if (!banks.ok) s3test::fail(__FILE__, __LINE__, banks.error);
    auto evaluator_result = s3::game::animation_pose::PoseEvaluator::from_banks(banks.value);
    const auto& evaluator = evaluator_result.unwrap();
    for (const bool held : {false, true}) {
        for (const bool running : {false, true}) {
            const std::string family = held ? "BR" : "NB";
            const std::string selection =
                "SS_" + family + "_JUMP_" + (running ? "RUN_INTO" : "STAND_0_INTO");
            auto metadata = banks.value.metadata();
            if (!metadata.ok) s3test::fail(__FILE__, __LINE__, metadata.error);
            auto tree = metadata.value.tree(selection);
            if (!tree.ok) s3test::fail(__FILE__, __LINE__, tree.error);
            const auto* const* space_ptr = std::get_if<const s3::data::SelectionSpaceMetadata*>(&tree.value);
            if (space_ptr == nullptr) S3_PANIC("Expected stock jump selection space");
            const s3::data::SelectionSpaceMetadata& space = **space_ptr;
            std::array<std::size_t, 5> indices{};
            const char* const parameter_names[] = {"CadenceStartPercent", "AnimTime", "AnimTransX", "AnimTransY",
                                                   "AnimTransZ"};
            for (std::size_t k = 0; k < 5; ++k) {
                bool found = false;
                for (std::size_t p = 0; p < space.parameters.size(); ++p) {
                    if (s3::data::eq_ignore_ascii_case(space.parameters[p].name, parameter_names[k])) {
                        indices[k] = p;
                        found = true;
                        break;
                    }
                }
                if (!found) S3_PANIC("called `Option::unwrap()` on a `None` value");
            }
            const s3::data::SelectionCandidateMetadata* candidate = nullptr;
            for (const auto& c : space.candidates) {
                if (c.child.rfind("J" + family + "_", 0) != 0) continue;
                if (!(rs::from_bits(c.value_bits[indices[1]]) > 0.0f)) continue;
                float sum = -0.0f;
                for (std::size_t k = 2; k < 5; ++k) sum += rs::powi(rs::from_bits(c.value_bits[indices[k]]), 2);
                if (sum < 4.0f) {
                    candidate = &c;
                    break;
                }
            }
            if (candidate == nullptr) S3_PANIC("Stock candidate inside native displacement cap");
            const std::string selected = candidate->child;
            std::array<float, 5> values{};
            for (std::size_t k = 0; k < 5; ++k) values[k] = rs::from_bits(candidate->value_bits[indices[k]]);
            const std::array<float, 4> translation{values[2], values[3], values[4], 0.0f};
            const std::size_t split = selected.find("_TO_");
            if (split == std::string::npos) S3_PANIC("called `Option::unwrap()` on a `None` value");
            const std::string suffix = selected.substr(split + 4);
            const std::string landing =
                family + "_LAND_" + suffix + "_INTO_" + (running ? "RUN_FWD" : "STAND");
            CHECK_MSG(metadata.value.tree(landing).ok, "Stock land.xml counterpart exists");
            MotionAnimation animation = MotionAnimation::from_metadata(std::move(metadata.value));
            //Explicit initialized owner, as in the stock body-tweak fixture.
            animation.skater_animation_flags = 0u;
            auto transition = [](float seconds) { return TransitionSettings{1, seconds, 0, 0, false}; };
            animation.play(PlaybackRequest{selection, 1.0f, 0.0f, transition(0.1f)}).unwrap();
            State state;
            state.begin(values[0], values[1], translation);
            state.update(animation, values[1], values[1], translation);
            animation.apply_parameters().unwrap();
            animation.advance(1.0f / 60.0f, 0.0f);
            animation.refresh_tree_attributes().unwrap();
            bool identity = false;
            for (const auto& a : animation.tree_attributes()) identity = identity || a.name == encode(selected);
            CHECK_MSG(identity, "Selected clip lost landing identity: " << selected);
            const std::vector<PoseCommand> first = animation.evaluate_pose(Evaluation{0.01f, true}).unwrap();
            const std::vector<Sqt> first_pose = evaluator.evaluate(first).unwrap();
            const float length = animation.current_length().unwrap();
            state.update(animation, values[1] * 0.5f, values[1], translation);
            animation.apply_parameters().unwrap();
            animation.advance(1.0f / 60.0f, 0.0f);
            const std::vector<PoseCommand> midway = animation.evaluate_pose(Evaluation{0.01f, true}).unwrap();
            const float expected_time = rs::min(length * 0.5f + 1.0f / 60.0f, length);
            bool at_time = false;
            for (const auto& c : midway) {
                if (const auto* clip = std::get_if<PoseCommand::Clip>(&c.value)) {
                    at_time = at_time || (clip->name == selected && std::fabs(clip->time - expected_time) < 0.0001f);
                }
            }
            CHECK(at_time);
            const std::vector<Sqt> midway_pose = evaluator.evaluate(midway).unwrap();
            bool moved = false;
            for (std::size_t j = 0; j < first_pose.size() && j < midway_pose.size(); ++j) {
                moved = moved || first_pose[j].rotation != midway_pose[j].rotation ||
                        first_pose[j].translation != midway_pose[j].translation;
            }
            CHECK(moved);
            //Named stock landing tree and .05 blend; direct selection is NOT
            //a substitute for the parent's full graph/physics regression.
            animation.play(PlaybackRequest{landing, 1.0f, 0.0f, transition(0.05f)}).unwrap();
            for (int i = 0; i < 4; ++i) {
                animation.apply_parameters().unwrap();
                animation.advance(1.0f / 60.0f, 0.0f);
                animation.refresh_tree_attributes().unwrap();
                const std::vector<PoseCommand> commands = animation.evaluate_pose(Evaluation{0.01f, true}).unwrap();
                bool landing_clip = false;
                for (const auto& c : commands) {
                    if (const auto* clip = std::get_if<PoseCommand::Clip>(&c.value)) {
                        landing_clip = landing_clip || clip->name == landing;
                    }
                }
                CHECK(landing_clip);
                CHECK(finite(evaluator.evaluate(commands).unwrap()));
            }
        }
    }
}
