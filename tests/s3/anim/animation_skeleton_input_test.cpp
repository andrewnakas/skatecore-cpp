// Port of skate-core/src/animation/skeleton_input/tests.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/packet_reset.hpp"
#include "s3/core/animation/skeleton_input.hpp"

using namespace s3::animation::skeleton_input;
using namespace s3::animation::skeleton_input::attribute_finalization;
using namespace s3::animation::skeleton_input::contact_events;
using namespace s3::animation::output::attributes;
using s3::animation::output::NativeMatrix;
using s3::animation::output::packet_reset::RESET_POSE;
using catalog::SCALAR_ATTRIBUTES;
using extended_attributes::ExtendedAttributes;
using name::encode;
using process_attributes::process;
using scalar_attributes::AnimationControlOutput;
using scalar_attributes::ScalarAttributeInputs;

namespace {
struct Map : s3::input::controller::ActionMap {
    float value(std::uint32_t key) override {
        switch (key) {
            case 64: return 0.25f;
            case 65: return -0.75f;
            case 68: return 0.5f;
            default: S3_PANIC(("Unexpected source action" + std::to_string(key)).c_str());
        }
    }
    std::uint8_t state(std::uint32_t) override {
        S3_PANIC("Attribute finalization reads analog values only");
    }
};
}  // namespace

S3_TEST(complete_attribute_packet_keeps_contact_timing_reset_and_finalization) {
    ScalarAttributeInputs fields = ScalarAttributeInputs::reset(8, UINT32_MAX);
    ExtendedAttributes extra = ExtendedAttributes::reset(0.0f);
    ContactEventState contacts{0, 0.0f};
    JumpAttributeState cached = JumpAttributeState::new_();
    AnimationControlOutput output{encode(""), 0};
    const std::array<AttributeName, 2> names = {encode("Trajectory"), encode("RightToeBase")};
    std::array<NativeMatrix, 2> hierarchy = {RESET_POSE, RESET_POSE};
    hierarchy[0][3] = {0.0f, 0.0f, 0.1f, 0.0f};
    // Deliberately unrelated foot position: native speed is trajectory/dt.
    hierarchy[1][3] = {20.0f, 10.0f, -40.0f, 0.0f};
    const ContactEventPose pose{names, hierarchy, 0, 1, 1.0f / 60.0f};
    Map map;
    FinalizationInput input{};
    input.select_jump_extremes = true;
    input.low_jump_threshold = 0.25f;
    input.high_jump_threshold = 0.75f;
    input.allow_height_override = true;
    input.use_prepared_controls = false;
    input.external_impulse_active = false;
    input.animation_flags = 0x08000000u;
    AnimationAttribute event = MotionGraphAttribute{encode("push_contact"), 0.0f}.to_animation();
    event.kind = 3;
    event.status = 4;
    for (std::size_t i = 0; i < 6; ++i) {
        event.payload._0[i] = i < 5 ? names[1]._0[i] : s3::rs::to_bits(1.0f);
    }
    {
        const std::array<AnimationAttribute, 1> events = {event};
        process(events, pose, fields, extra, contacts, cached, output, input, &map).unwrap();
    }
    CHECK_EQ(contacts.bone, 1);
    CHECK(s3::rs::abs(contacts.push_speed - 6.0f) < 0.00001f);
    CHECK_EQ(fields.flags2468 & 0x0f000000u, 0x0e000000u);
    CHECK_EQ(extra.flags2480 & 8u, 8u);  // byte at start of a big-endian word
    CHECK((extra.jump_controls == std::array<float, 2>{0.25f, 0.5f}));
    event.status = 2;
    contacts.push_speed = 7.0f;
    {
        const std::array<AnimationAttribute, 1> events = {event};
        process(events, pose, fields, extra, contacts, cached, output, input, &map).unwrap();
    }
    CHECK_EQ(contacts.push_speed, 7.0f);  // inactive event performs no writes
    std::vector<AnimationAttribute> attributes;
    for (const auto& a : SCALAR_ATTRIBUTES) {
        attributes.push_back(MotionGraphAttribute{a.encoded_name, 0.5f}.to_animation());
    }
    process(attributes, pose, fields, extra, contacts, cached, output, input, &map).unwrap();
    CHECK((cached.prepared_controls == std::array<float, 2>{0.25f, -0.75f}));
    CHECK_EQ(fields.flags2476 & 2u, 2u);  // PlayerControlledPump
    CHECK_EQ(fields.flags2476 & 0x400000u, 0u);  // retrieving/dropping clears GrabWorld
    const ScalarAttributeInputs reset = ScalarAttributeInputs::reset(fields.flags2468, fields.flags2488);
    CHECK_EQ(reset.flags2468, 0x2008u);
    CHECK_EQ(reset.turn_scale, 1.0f);
    CHECK_EQ(reset.magnitude_scale, 1.0f);
    CHECK_EQ(reset.cadence_end_percent, -1.0f);
}
