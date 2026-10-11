// Port of skate-game/src/graph_host/motion_grind/tests.rs and tests/{conditions,fixture}.rs.
// tests/routing.rs exercises motion_nodes::MotionFactory (ported with motion_nodes.rs, not here).
#include <string>
#include <utility>
#include <vector>

#include "harness.hpp"
#include "s3/core/animation/phase_blend.hpp"
#include "s3/core/animation/playback.hpp"
#include "s3/core/animation/playback_clip.hpp"
#include "s3/core/animation/playback_transition.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/graph/intents.hpp"
#include "s3/game/graph_host/motion_grind.hpp"

using namespace s3::game::graph_host::motion_grind;
using s3::animation::output::attributes::AnimationAttribute;
using s3::animation::output::attributes::MotionGraphAttribute;
using s3::animation::playback::TransitionSettings;
using s3::animation::playback_clip::ClipAttribute;
using s3::animation::playback_clip::PlaybackClip;
using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::playback_parameters::SettableAttributes;
using s3::animation::playback_transition::PlaybackTransition;
using s3::animation::playback_tree::PlaybackTree;
using s3::animation::skeleton_input::name::encode;
using s3::data::GraphAttribute;
using s3::data::state_graph_attributes::Attributes;
using s3::graph::intents::IntentMap;
namespace phase_blend = s3::animation::phase_blend;
namespace rs = s3::rs;

namespace {

// ---- tests/fixture.rs: independent synthetic data, using the genuine core playback pipeline.
struct Owner {
    PlaybackTree tree;
    SettableAttributes pending;
    std::vector<MotionGraphAttribute> packets;
    IntentMap intents;
    std::size_t applications = 0;

    explicit Owner(PlaybackTree tree_) : tree(std::move(tree_)) {}

    void set_attribute(SettableAttribute a) { pending.set_attribute(a); }
    rs::Result<rs::Unit, std::string> apply_parameters() {
        auto r = tree.set_attributes(pending.entries());
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        pending.clear();
        applications += 1;
        return rs::Unit{};
    }
    rs::Result<bool, std::string> query_current_attribute(AttributeName name, std::uint32_t mask,
                                                          AnimationAttribute& out) const {
        return tree.query_attribute(name, mask, out);
    }
    void emit_packet(AttributeName name, float value) { packets.push_back(MotionGraphAttribute{name, value}); }
    std::optional<float> intent(std::string_view name) const {
        const float* v = intents.get(name);
        return v ? std::optional<float>(*v) : std::nullopt;
    }
};

PlaybackTree clip(std::optional<float> twist, float height) {
    std::vector<ClipAttribute> attributes{ClipAttribute{encode("height"), 0, -1.0f, -1.0f, {rs::to_bits(height)}}};
    if (twist) attributes.push_back(ClipAttribute{encode("twist"), 0, -1.0f, -1.0f, {rs::to_bits(*twist)}});
    return PlaybackTree::Clip{"independent-test", PlaybackClip::new_(61.0f, 60.0f, 1.0f, 0, attributes)};
}
PlaybackTree blend() {
    return PlaybackTree::PhaseBlend{
        phase_blend::PhaseBlend::new_(encode("twist"), {clip(-2.0f, 0.0f), clip(4.0f, 0.0f)}).unwrap()};
}
PlaybackTree height_dependent_blend() {
    auto branch = [](float a, float b) {
        return PlaybackTree(PlaybackTree::PhaseBlend{
            phase_blend::PhaseBlend::new_(encode("height"), {clip(a, 0.0f), clip(b, 1.0f)}).unwrap()});
    };
    return PlaybackTree::PhaseBlend{
        phase_blend::PhaseBlend::new_(encode("twist"), {branch(-1.0f, -2.0f), branch(2.0f, 4.0f)}).unwrap()};
}

// ---- tests.rs helpers
std::vector<GraphAttribute> attributes(std::initializer_list<std::pair<const char*, const char*>> pairs) {
    std::vector<GraphAttribute> out;
    for (const auto& [name, text] : pairs) {
        GraphAttribute a;
        a.name = name;
        a.text = text;
        a.float_bits = 0;
        a.boolean_byte = 0;
        out.push_back(a);
    }
    return out;
}
Settings grind_settings() {
    Settings s{};
    s.height = {0.25f, 1.0f, 2.0f};
    s.fade.response = 1.0f;
    s.fade.input_scale = 1.0f;
    s.fade.acceleration = 0.25f;
    s.fade.maximum_step = 0.5f;
    return s;
}
Physical physical() {
    return Physical{true,  encode("FS_50_50"), {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, false, false, 1.0f,
                    0.75f, 0.0f};
}

}  // namespace

S3_TEST(parser_preserves_authored_crouch_name_and_rejects_missing_fade_attributes) {
    auto a = attributes({{"name", "ControlGrindCrouch"}, {"driveDistToCom", "CustomHeight"}});
    CHECK(Operation::parse(Attributes(a)).unwrap() ==
          std::optional<Operation>(Operation::Crouch{encode("CustomHeight")}));
    a = attributes({{"name", "GrindControlFade"}});
    CHECK(Operation::parse(Attributes(a)).is_err());
}

S3_TEST(creates_ordered_packets_not_intents_or_settable_parameters) {
    Owner a(blend());
    State state;
    a.intents.insert("retained", 0.5f);
    const IntentMap before = a.intents;
    const Operation op = Operation::Attributes{};
    execute(state, op, 0, 0.1f, grind_settings(), physical(), a).unwrap();
    CHECK(a.packets.empty());
    execute(state, op, 1, 0.1f, grind_settings(), physical(), a).unwrap();
    CHECK(a.packets == (std::vector<MotionGraphAttribute>{{encode("FS_50_50"), 1.0f},
                                                          {encode("GrindFacingBackwards"), 1.0f}}));
    CHECK(a.intents.values == before.values);
    CHECK(a.pending.entries().is_empty());
    Physical p = physical();
    p.grinding = false;
    execute(state, op, 1, 0.1f, grind_settings(), p, a).unwrap();
    CHECK_EQ(a.packets.size(), std::size_t(2));
}

S3_TEST(crouch_begins_from_physical_height_updates_authored_parameter_and_ends_silently) {
    Owner a(blend());
    State state;
    const Operation op = Operation::Crouch{encode("custom")};
    CHECK(execute(state, op, 1, 0.125f, grind_settings(), physical(), a).is_err());
    execute(state, op, 0, 0.125f, grind_settings(), physical(), a).unwrap();
    CHECK(a.pending.entries().is_empty());
    execute(state, op, 1, 0.125f, grind_settings(), physical(), a).unwrap();
    const SettableAttribute entry = a.pending.entries()[0];
    CHECK(entry.name == encode("custom"));
    CHECK_EQ(entry.value, 0.75f);
    CHECK_EQ(entry.normalized, false);
    CHECK_EQ(entry.sequence_id, -1);
    a.pending.clear();
    execute(state, op, 2, 0.125f, grind_settings(), physical(), a).unwrap();
    CHECK(a.pending.entries().is_empty());
}

S3_TEST(fade_begin_applies_height_before_endpoints_and_keeps_live_tree_effects) {
    Owner a(height_dependent_blend());
    State state;
    const Operation op = Operation::Fade{encode("height"), encode("twist"), "balance"};
    Physical p = physical();
    p.twist = 3.0f;
    execute(state, op, 0, 0.1f, grind_settings(), p, a).unwrap();
    CHECK_EQ(a.applications, std::size_t(2));
    const auto& f = *state.fade;
    CHECK_EQ(f.minimum, -2.0f);
    CHECK_EQ(f.maximum, 4.0f);
    CHECK_EQ(f.value, 3.0f);
    CHECK_EQ(a.pending.entries().len(), std::size_t(1));
    CHECK(!a.pending.entries()[0].normalized);
    a.apply_parameters().unwrap();
    AnimationAttribute out = MotionGraphAttribute{encode("twist"), 0.0f}.to_animation();
    CHECK(a.query_current_attribute(encode("twist"), 15, out).unwrap());
    CHECK_EQ(rs::from_bits(*out.payload._0[0]), 3.0f);
    execute(state, op, 1, 0.1f, grind_settings(), p, a).unwrap();
    CHECK_MSG(a.pending.entries().is_empty(), "first Update emitted a new value");
}

S3_TEST(endpoint_miss_keeps_original_zero_or_partial_output) {
    Owner missing(clip(std::nullopt, 0.0f));
    CHECK(animation::endpoints(missing, encode("twist")).unwrap() == (std::array<float, 2>{0.0f, 0.0f}));
    Owner partial(PlaybackTree::PhaseBlend{
        phase_blend::PhaseBlend::new_(encode("height"), {clip(-2.0f, 0.0f), clip(std::nullopt, 1.0f)}).unwrap()});
    CHECK(animation::endpoints(partial, encode("twist")).unwrap() == (std::array<float, 2>{-2.0f, -2.0f}));
}

S3_TEST(endpoint_root_query_respects_sequence_vs_blend_without_forcing_destination) {
    const std::pair<std::uint32_t, std::array<float, 2>> cases[] = {{2, {-2.0f, 4.0f}}, {4, {-9.0f, -9.0f}}};
    for (const auto& [kind, expected] : cases) {
        const PlaybackTransition transition =
            PlaybackTransition::new_(clip(-9.0f, 0.0f), blend(), TransitionSettings{kind, 0.5f, 0, 0, false});
        Owner a(PlaybackTree::Transition{transition});
        CHECK(animation::endpoints(a, encode("twist")).unwrap() == expected);
    }
}

// ---- tests/conditions.rs
namespace {
conditions::Physical condition_physical() { return conditions::Physical{false, 4, 1, 2, true, 0.125f, true}; }
}  // namespace

S3_TEST(conditions_use_their_actual_fields_not_a_shared_grinding_proxy) {
    using conditions::Condition;
    conditions::Physical p = condition_physical();
    CHECK(Condition(Condition::BluntingBackslash{}).evaluate(p));
    CHECK(Condition(Condition::TrickOutType{2}).evaluate(p));
    CHECK(Condition(Condition::LandingIntoGrind{}).evaluate(p));
    CHECK(Condition(Condition::DroppingIn{}).evaluate(p));
    CHECK(!Condition(Condition::Approach{true}).evaluate(p));
    p.filtered_grinding_80 = true;
    CHECK(Condition(Condition::Approach{true}).evaluate(p));
    CHECK(!Condition(Condition::Approach{false}).evaluate(p));
    p.approach_268 = 0;
    CHECK(Condition(Condition::Approach{false}).evaluate(p));
    p.air_time_184 = 0.2f;
    CHECK(!Condition(Condition::LandingIntoGrind{}).evaluate(p));
    p.air_time_184 = 0.0f;
    p.air_grind_443 = false;
    CHECK(!Condition(Condition::LandingIntoGrind{}).evaluate(p));
}

S3_TEST(approach_case_folds_facing_but_trick_out_type_uses_exact_strings) {
    using conditions::Condition;
    auto a = attributes({{"name", "IsGrindApproach"}, {"Facing", "f"}});
    CHECK(Condition::parse(Attributes(a)).unwrap() == std::optional<Condition>(Condition::Approach{true}));
    const std::pair<const char*, std::uint32_t> cases[] = {{"normal", 0}, {"left", 1}, {"right", 2}};
    for (const auto& [name, value] : cases) {
        a = attributes({{"name", "GrindTrickOutTypeAllowed"}, {"type", name}});
        CHECK(Condition::parse(Attributes(a)).unwrap() == std::optional<Condition>(Condition::TrickOutType{value}));
    }
    a = attributes({{"name", "GrindTrickOutTypeAllowed"}, {"type", "Left"}});
    CHECK(Condition::parse(Attributes(a)).is_err());
}
