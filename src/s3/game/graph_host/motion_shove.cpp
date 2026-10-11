// Port of skate-game/src/graph_host/motion_shove.rs (module `motion_shove`).
#include "s3/game/graph_host/motion_shove.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/input/angle.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_shove {

using s3::animation::channel_playback::ChannelSettings;
using s3::animation::playback::TransitionSettings;
using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::skeleton_input::name::encode;

namespace {
void set(MotionAnimation& animation, const char* name, float value) {
    animation.set_attribute(SettableAttribute{encode(name), value, false, -1});
}

rs::Result<rs::Unit, std::string> start_channel(MotionAnimation& animation, const char* channel,
                                                const std::string& tree, bool anticipation) {
    const float seconds = rs::from_bits(anticipation ? 0x3e4ccccdu : 0x3dcccccdu);
    ChannelSettings settings;
    settings.priority = 0;
    settings.keep_alive = anticipation;
    settings.mirrored = false;
    settings.speed = 1.0f;
    settings.blend_in = seconds;
    settings.hold_during_blend_in = false;
    settings.blend_out = seconds;
    settings.hold_during_blend_out = false;
    settings.use_attributes = false;
    //Vtable16 is TransitionTo82D1D090. Stack booleans at87/95/103 are1:
    //allow resurrection, create missing and use channel weights.
    const TransitionSettings transition{2, rs::from_bits(0x3dcccccdu), 0, 0, true};
    S3_MOTION_CHECK(animation.transition_channel(channel, tree, settings, transition, true, true));
    return rs::Unit{};
}
}  // namespace

rs::Result<std::optional<ShoveOperation>, std::string> ShoveOperation::parse(const Attributes& a) {
    const std::string* name = a.text("name");
    if (name == nullptr || *name != "Shove") return std::optional<ShoveOperation>();
    //The required lookups are virtual8; virtual4 supplies empty defaults
    //only for the two board-specific alternatives.
    const std::string* selection = a.text("Selection");
    if (selection == nullptr) return rs::make_err(std::string("Shove requires Selection"));
    const std::string* selection_board = a.text("SelectionBrd");
    const std::string* anticipation = a.text("Antic");
    if (anticipation == nullptr) return rs::make_err(std::string("Shove requires Antic"));
    const std::string* anticipation_board = a.text("AnticBrd");
    return std::optional<ShoveOperation>(ShoveOperation{*selection, selection_board ? *selection_board : std::string(),
                                                        *anticipation,
                                                        anticipation_board ? *anticipation_board : std::string()});
}

rs::Result<rs::Unit, std::string> ShoveState::update(const ShoveOperation& operation, MotionAnimation& animation,
                                                     ShovePhysical physical, std::array<std::uint32_t, 2> busy_hands,
                                                     bool mirrored) {
    const bool retrieving = animation.channels.has("RetrieveBoard");
    const bool board_variant = physical.in_biped_category && physical.board_on_ground;
    if (animation.motion_intent("GrabWorld").has_value() && !retrieving && busy_hands[0] == 0 && busy_hands[1] == 0) {
        if (!animation.channels.has("SkitchAntic") && !animation.channels.has("Shove")) {
            board_anticipation = board_variant;
            S3_MOTION_CHECK(start_channel(animation, "SkitchAntic",
                                          board_variant ? operation.anticipation_board : operation.anticipation,
                                          true));
        }
    } else if (animation.channels.has("SkitchAntic")) {
        animation.channels.end("SkitchAntic");
    }

    bool shove = animation.channels.has("Shove");
    if (!physical.interaction_trigger || retrieving) {
        if (shove) {
            if (retrieving) {
                animation.channels.end("Shove");
            } else {
                set(animation, "ShoveDirection", angle);
            }
        }
        return rs::Unit{};
    }
    angle = direction_angle(physical.direction, mirrored);
    if (animation.channels.has("SkitchAntic")) animation.channels.end("SkitchAntic");
    if (!shove) {
        S3_MOTION_CHECK(start_channel(animation, "Shove",
                                      board_variant ? operation.selection_board : operation.selection, false));
        //Original marks this true after the call, rather than querying
        //the channel manager a second time during the same update.
        shove = true;
    }
    if (shove) {
        set(animation, "GstrDistToCog", physical.animation_height);
        set(animation, "ShoveDirection", angle);
    }
    return rs::Unit{};
}

void ShoveState::end(MotionAnimation& animation, bool keep_channels) {
    //SpecificMotionGraph virtual212=8258FC30, bit18. Native reset clears it.
    //Virtual44 is EndChannelPrematurely82D1D6A8, preserving fade timing.
    if (!keep_channels) {
        for (const char* name : {"SkitchAntic", "Shove"}) {
            if (animation.channels.has(name)) animation.channels.end(name);
        }
    }
}

float direction_angle(std::array<float, 4> direction, bool mirrored) {
    const float x = direction[0];
    const float z = mirrored ? -direction[2] : direction[2];
    float reciprocal = rs::recip(z);
    reciprocal = rs::mul_add(reciprocal, rs::mul_add(-reciprocal, z, 1.0f), reciprocal);
    const float basic = s3::input::angle::atan(rs::mul_add(x, reciprocal, 0.0f));
    const std::uint32_t sign = rs::to_bits(x) & 0x80000000u;
    float angle = z < 0.0f ? basic + rs::from_bits(0x40490fdbu | sign) : basic;
    angle = z == 0.0f ? rs::from_bits(0x3fc90fdbu | sign) : angle;
    const float degrees = angle * rs::from_bits(0x42652ee1u);
    return degrees < 0.0f ? degrees + rs::from_bits(0x43b40000u) : degrees;
}

}  // namespace s3::game::graph_host::motion_shove
