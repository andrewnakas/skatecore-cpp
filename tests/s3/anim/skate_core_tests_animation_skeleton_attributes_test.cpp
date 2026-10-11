// Port of skate-core/tests/animation_skeleton_attributes.rs (crate integration test).
#include <string_view>

#include "harness.hpp"
#include "s3/core/animation/skeleton_input.hpp"

using namespace s3::animation::skeleton_input;
using s3::animation::output::attributes::AnimationAttribute;
using s3::animation::output::attributes::AttributeName;
using s3::animation::output::attributes::AttributePayload;
using catalog::EVENT_COMPARISONS;
using catalog::SCALAR_ATTRIBUTES;
using name::encode;
using scalar_attributes::AnimationControlOutput;
using scalar_attributes::DispatchError;
using scalar_attributes::ScalarAttributeInputs;

namespace {
ScalarAttributeInputs input() {
    ScalarAttributeInputs i;
    i.flags2468 = 0x1000;
    i.flags2472 = 0x1000;
    i.flags2476 = 0x1000;
    i.flags2484 = 0x1000;
    i.flags2488 = 0x1000;
    i.board_adjust = 999;
    i.balance = 2.0f;
    i.spin = 3.0f;
    i.body_spin = 4.0f;
    i.brake = 5.0f;
    i.turn = 6.0f;
    i.turn_scale = 7.0f;
    i.magnitude_scale = 8.0f;
    i.animation_end_com = {1.0f, 2.0f, 3.0f, -0.0f};
    i.animation_translation = {4.0f, 5.0f, 6.0f, 9.0f};
    i.animation_time = 10.0f;
    i.animation_physics_blend_seconds = 11.0f;
    i.cadence_end_percent = 12.0f;
    i.raw_turn = 13.0f;
    i.hard_turn = 14.0f;
    i.slide = 15.0f;
    return i;
}
AnimationControlOutput output() { return AnimationControlOutput{AttributeName{{99, 99, 99, 99, 99}}, 0x1000}; }
AnimationAttribute attribute(std::string_view name, std::optional<float> value) {
    AnimationAttribute a;
    a.name = encode(name);
    a.payload = AttributePayload{};
    if (value) a.payload._0[0] = s3::rs::to_bits(*value);
    a.begin_time = 123.0f;
    a.end_time = -99.0f;
    a.status = 0;
    a.kind = 0;
    a.sequence_id = 5;
    return a;
}
void apply(std::string_view name, std::optional<float> value, ScalarAttributeInputs& i, AnimationControlOutput& o) {
    scalar_attributes::dispatch_attribute(attribute(name, value), i, o).unwrap();
}
}  // namespace

S3_TEST(native_name_encoding_is_base38_case_insensitive_chunked_and_nul_terminated) {
    CHECK((encode("A")._0 == std::array<std::uint32_t, 5>{11u * 79235168u, 0, 0, 0, 0}));
    CHECK_EQ(encode("a0_zZz")._0[0], 11u * 79235168u + 2085136u + 37u * 54872u + 36u * 1444u + 36u * 38u + 36u);
    CHECK_EQ(encode("ABCDEFg")._0[1], 17u * 79235168u);
    CHECK(encode("Turn") == encode("tUrN"));
    CHECK(encode(std::string_view("Turn\0Brake", 10)) == encode("Turn"));
    CHECK(encode("123456123456123456123456123456ignored") == encode("123456123456123456123456123456"));
    const std::uint8_t ff[1] = {255};
    CHECK_EQ(encode(ff, 1)._0[0], static_cast<std::uint32_t>(-48) * 79235168u);
    CHECK(encode("push_contact") != encode("PushContact"));
}

S3_TEST(complete_dispatch_identity_catalog_retains_direct_and_event_comparisons) {
    CHECK_EQ(SCALAR_ATTRIBUTES.size(), std::size_t(151));
    CHECK_EQ(EVENT_COMPARISONS.size(), std::size_t(5));
    for (std::size_t k = 0; k + 1 < SCALAR_ATTRIBUTES.size(); ++k) {
        CHECK(SCALAR_ATTRIBUTES[k].comparison_site < SCALAR_ATTRIBUTES[k + 1].comparison_site);
    }
    CHECK_EQ(catalog::lookup(encode("Spin"))->comparison_site, 0x82bdb004u);
    CHECK_EQ(catalog::lookup(encode("IsWipeoutPushOffImpulse"))->comparison_site, 0x82bdb148u);
    CHECK_EQ(catalog::lookup(encode("WantsLeftAirGrab"))->comparison_site, 0x82bdb5fcu);
    CHECK_EQ(catalog::lookup(encode("WantsRightAirGrab"))->comparison_site, 0x82bdb614u);
    CHECK_EQ(catalog::lookup(encode("jump"))->comparison_site, 0x82bdb7e8u);
    CHECK(catalog::lookup(encode("push_contact")) == nullptr);
}

S3_TEST(steering_and_push_markers_write_their_distinct_consumed_fields_without_thresholds) {
    ScalarAttributeInputs i = input();
    AnimationControlOutput o = output();
    for (std::string_view name : {"NoInput", "NewAutoPump", "ManualBrake", "Carving", "PushContact", "PushSpeed"}) {
        apply(name, std::nullopt, i, o);
    }
    apply("Brake", -2.5f, i, o);
    apply("Turn", 0.25f, i, o);
    apply("Spin", -0.75f, i, o);
    CHECK_EQ(i.brake, -2.5f);
    CHECK_EQ(i.turn, -0.25f);
    CHECK_EQ(i.spin, 0.75f);
    CHECK_EQ(i.flags2468, 0xa0001000u);
    CHECK_EQ(i.flags2472, 0x00801000u);
    CHECK_EQ(i.flags2488, 0x20801000u);
    CHECK_EQ(i.flags2476, 0x1000u);
    CHECK_EQ(o.flags, 0x60001000u);
    CHECK_EQ(i.balance, 2.0f);
    // A false/zero scalar is still a presence marker in these native branches.
    apply("PushContact", 0.0f, i, o);
    CHECK_EQ(i.flags2488, 0x20801000u);
}

S3_TEST(repeated_attributes_preserve_list_order_and_do_not_clear_unrelated_state) {
    ScalarAttributeInputs i = input();
    AnimationControlOutput o = output();
    apply("SlideBoard", std::nullopt, i, o);
    apply("GrindNose", std::nullopt, i, o);
    apply("GrindFacingForwards", std::nullopt, i, o);
    apply("GrindFacingBackwards", std::nullopt, i, o);
    apply("BoardAdjustLeft", std::nullopt, i, o);
    apply("BoardAdjustDown", std::nullopt, i, o);
    apply("Turn", 1.0f, i, o);
    apply("TURN", -0.0f, i, o);
    CHECK(o.grind_name == encode("GrindNose"));
    CHECK_EQ(o.flags, 0x1000u);
    CHECK_EQ(i.board_adjust, 4u);
    CHECK_EQ(s3::rs::to_bits(i.turn), 0u);
    CHECK_EQ(i.brake, 5.0f);
    CHECK_EQ(i.flags2468, 0x1000u);
}

S3_TEST(com_translation_and_scale_handlers_preserve_other_lanes_and_float_signs) {
    ScalarAttributeInputs i = input();
    AnimationControlOutput o = output();
    apply("AnimEndCOMY", -7.0f, i, o);
    apply("AnimTransZ", 8.0f, i, o);
    apply("TurnScale", 0.0f, i, o);
    apply("MagScale", -3.0f, i, o);
    apply("RawTurn", s3::rs::from_bits(0x7fc12345u), i, o);
    CHECK_EQ(i.animation_end_com[0], 1.0f);
    CHECK_EQ(i.animation_end_com[1], -7.0f);
    CHECK_EQ(i.animation_end_com[2], 3.0f);
    CHECK_EQ(s3::rs::to_bits(i.animation_end_com[3]), 0x80000000u);
    CHECK((i.animation_translation == std::array<float, 4>{4.0f, 5.0f, 8.0f, 9.0f}));
    CHECK_EQ(i.turn_scale, 0.0f);
    CHECK_EQ(i.magnitude_scale, -3.0f);
    CHECK_EQ(s3::rs::to_bits(i.raw_turn), 0xffc12345u);
}

S3_TEST(scalar_kind_gate_ignores_timing_status_but_never_treats_active_events_as_scalars) {
    ScalarAttributeInputs i = input();
    AnimationControlOutput o = output();
    const ScalarAttributeInputs original = i;
    for (std::uint8_t kind : {1, 2, 127, 128, 255}) {
        AnimationAttribute a = attribute("Turn", 0.75f);
        a.kind = kind;
        a.status = 255;
        scalar_attributes::dispatch_attribute(a, i, o).unwrap();
        CHECK(i == original);
    }
    for (std::uint8_t status : {0, 1, 2, 16, 128}) {
        AnimationAttribute a = attribute("Turn", std::nullopt);
        a.kind = 3;
        a.status = status;
        scalar_attributes::dispatch_attribute(a, i, o).unwrap();
        CHECK(i == original);
    }
    for (std::uint8_t status : {4, 8, 12, 255}) {
        AnimationAttribute a = attribute("unknown-event", std::nullopt);
        a.kind = 3;
        a.status = status;
        const auto r = scalar_attributes::dispatch_attribute(a, i, o);
        CHECK(r.is_err() && std::get_if<DispatchError::EventConsumerUnavailable>(&r.unwrap_err().value));
    }
    AnimationAttribute a = attribute("Turn", 2.0f);
    a.status = 255;
    scalar_attributes::dispatch_attribute(a, i, o).unwrap();
    CHECK_EQ(i.turn, -2.0f);
}

S3_TEST(unported_known_handlers_and_uninitialized_scalars_fail_without_writes) {
    ScalarAttributeInputs i = input();
    AnimationControlOutput o = output();
    const ScalarAttributeInputs original = i;
    const AnimationControlOutput original_o = o;
    {
        const auto r = scalar_attributes::dispatch_attribute(attribute("MinJump", 0.8f), i, o);
        const auto* e = r.is_err() ? std::get_if<DispatchError::KnownScalarUnavailable>(&r.unwrap_err().value) : nullptr;
        CHECK(e && e->attribute.comparison_site == 0x82bdb71cu);
    }
    {
        const auto r = scalar_attributes::dispatch_attribute(attribute("Turn", std::nullopt), i, o);
        CHECK(r.is_err() && std::get_if<DispatchError::UninitializedScalar>(&r.unwrap_err().value));
    }
    scalar_attributes::dispatch_attribute(attribute("unknown-name", std::nullopt), i, o).unwrap();
    CHECK(i == original);
    CHECK(o == original_o);
}
