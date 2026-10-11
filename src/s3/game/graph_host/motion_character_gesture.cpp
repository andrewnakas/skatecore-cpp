// Port of skate-game/src/graph_host/motion_character_gesture.rs (out-of-line items).
#include "s3/game/graph_host/motion_character_gesture.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::graph_host::motion_character_gesture {

using s3::animation::channel_playback::ChannelSettings;
using s3::animation::playback::TransitionSettings;
using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::skeleton_input::name::encode;

namespace {

constexpr std::array<const char*, 3> CHANNELS = {"GestureBoth", "GestureRight", "GestureLeft"};
constexpr float TENTH = rs::from_bits(0x3DCCCCCDu);

ChannelSettings settings(bool keep_alive, bool hold_in) {
    ChannelSettings s;
    s.priority = 0;
    s.keep_alive = keep_alive;
    s.mirrored = false;
    s.speed = 1.0f;
    s.blend_in = TENTH;
    s.hold_during_blend_in = hold_in;
    s.blend_out = TENTH;
    s.hold_during_blend_out = true;
    s.use_attributes = false;
    return s;
}

void end_all(MotionAnimation& animation) {
    for (const std::size_t index : {2u, 1u, 0u}) animation.channels.end(CHANNELS[index]);
}

bool has(const IntentMap& intents, const char* name) { return intents.contains_key(name); }

std::int32_t select_start(const IntentMap& intents) {
    const std::array<std::pair<std::int32_t, const char*>, 4> starts = {{
        {3, "GestureDownStart"},
        {0, "GestureLeftStart"},
        {2, "GestureUpStart"},
        {1, "GestureRightStart"},
    }};
    for (const auto& [direction, name] : starts) {
        if (has(intents, name)) return direction;
    }
    return -1;
}

///82BAB258: suppress occupied special channels/mount actions, then choose
///the actual available hand. Offboard states6/7 have a separate board rule.
std::int32_t select_hands(const MotionAnimation& animation, const CharacterGestureInputs& input) {
    bool occupied = false;
    for (const char* name : {"RetrieveBoard", "Shove", "WipeoutPushOff"}) occupied = occupied || animation.channels.has(name);
    if (occupied || has(input.motion_intents, "OB_Mount") || has(input.motion_intents, "OB_Dismount")) {
        return -1;
    }
    if ((input.filtered_state == 6 || input.filtered_state == 7) && !input.ground321) {
        return input.board_held311 ? 2 : 0;
    }
    const bool first = input.busy_hands[0] != 0;
    const bool second = input.busy_hands[1] != 0;
    if (!first && !second) return 0;
    if (!first && second) return 1;
    if (first && !second) return 2;
    return -1;
}

rs::Result<std::uint32_t, std::string> selection(std::int32_t direction,
                                                 const std::optional<std::array<std::uint32_t, 4>>& selections) {
    std::size_t index = 0;
    switch (direction) {
        case 0: index = 2; break;
        case 1: index = 3; break;
        case 2: index = 0; break;
        case 3: index = 1; break;
        default: return 37u;
    }
    if (!selections) return rs::make_err(std::string("CharacterGesture needs the actual skater gesture selections"));
    return (*selections)[index];
}

rs::Result<std::string, std::string> animation_name(std::int32_t direction, std::int32_t hands, std::int32_t stage,
                                                    const std::optional<std::array<std::uint32_t, 4>>& selections) {
    auto id = selection(direction, selections);
    if (id.is_err()) return rs::make_err(id.unwrap_err());
    if (id.unwrap() >= NAMES.size()) {
        return rs::make_err("Invalid stock gesture selection " + std::to_string(id.unwrap()));
    }
    static constexpr std::array<const char*, 3> HANDS = {"BOTH", "RIGHT", "LEFT"};
    static constexpr std::array<const char*, 3> STAGES = {"INTO", "CYC", "OUT"};
    return std::string(NAMES[id.unwrap()]) + "_" + HANDS.at(static_cast<std::size_t>(hands)) + "_" +
           STAGES.at(static_cast<std::size_t>(stage));
}

}  // namespace

const std::array<const char*, 37> NAMES = {
    "B_GSTR_AIRGUITAR",  "B_GSTR_AIRPLANE",   "B_GSTR_BOXING",    "B_GSTR_BRUCE_LEE",     "B_GSTR_CHECKTIME",
    "B_GSTR_DEVIL",      "B_GSTR_DBLE_GUN",   "B_GSTR_DUNNO",     "B_GSTR_FINGERWAG",     "B_GSTR_FISTS",
    "B_GSTR_BICEPFLEX",  "B_GSTR_FLIPTABLE",  "B_GSTR_FONZ_EH",   "B_GSTR_FREEDOM",       "B_GSTR_FU_FIST",
    "B_GSTR_GETAWAY",    "B_GSTR_GETOUTTAHERE", "B_GSTR_HANDCUFFS", "B_GSTR_HIGHPUMP",    "B_GSTR_LOWPUMP",
    "B_GSTR_PREWIND",    "B_GSTR_PEACE",      "B_GSTR_POINT",     "B_GSTR_RAISEROOF",     "B_GSTR_SHAKA",
    "B_GSTR_SHRUG",      "B_GSTR_POINT_SKY",  "B_GSTR_SNAP",      "B_GSTR_SOULARCH",      "B_GSTR_SPOCK",
    "B_GSTR_SURFSUP",    "B_GSTR_SWINGHIGH",  "B_GSTR_SWINGLOW",  "B_GSTR_THROWARMS",     "B_GSTR_THMB_DWN",
    "B_GSTR_WINGS",      "B_GSTR_YARDSALE",
};

void CharacterGesture::end(MotionAnimation& animation) {
    end_all(animation);
    clear_active();
    stage = -1;
}

rs::Result<std::optional<GesturePublication>, std::string> CharacterGesture::update(
    MotionAnimation& animation, const CharacterGestureInputs& input) {
    bool any_channel = false;
    for (const char* name : CHANNELS) any_channel = any_channel || animation.channels.has(name);
    if (active && !any_channel) {
        stage = -1;
        clear_active();
    } else if (!active && any_channel) {
        end_all(animation);
        clear_active();
    }
    const std::array<bool, 4> held = {
        has(input.motion_intents, "GestureLeftHeld"),
        has(input.motion_intents, "GestureRightHeld"),
        has(input.motion_intents, "GestureUpHeld"),
        has(input.motion_intents, "GestureDownHeld"),
    };
    auto held_at = [&](std::int32_t d) { return held.at(static_cast<std::size_t>(d)); };
    std::int32_t selected = select_start(input.motion_intents);
    const std::int32_t hands_now = select_hands(animation, input);
    if (selected == -1 && direction != -1 && !held_at(direction)) {
        selected = -1;
        for (const std::int32_t d : {3, 0, 2, 1}) {
            if (held_at(d)) {
                selected = d;
                break;
            }
        }
    }
    const bool braking = has(input.motion_intents, "ForceBrake") && !input.force_brake_bypass;
    const bool start = selected != -1 && hands_now != -1 && !braking && (!input.suppress_up || selected != 2) &&
                       (direction == -1 || !held_at(direction)) && (!active || selected != direction);
    if (start) {
        auto name = animation_name(selected, hands_now, 0, input.selections);
        if (name.is_err()) return rs::make_err(name.unwrap_err());
        direction = selected;
        stage = 0;
        TransitionSettings transition;
        transition.kind = 2;
        transition.seconds = rs::from_bits(0x3E4CCCCDu);
        transition.under = 0;
        transition.matching = 0;
        transition.use_channels_from_weights = false;
        auto r = animation.transition_channel(CHANNELS[static_cast<std::size_t>(hands_now)], name.unwrap(),
                                              settings(false, true), transition, true, true);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        //82BAA954..AA4C ends the other two channels, in source order.
        switch (hands_now) {
            case 0:
                animation.channels.end(CHANNELS[2]);
                animation.channels.end(CHANNELS[1]);
                break;
            case 1:
                animation.channels.end(CHANNELS[2]);
                animation.channels.end(CHANNELS[0]);
                break;
            case 2:
                animation.channels.end(CHANNELS[0]);
                animation.channels.end(CHANNELS[1]);
                break;
            default: S3_PANIC("internal error: entered unreachable code");
        }
        hands = hands_now;
        active = true;
    } else if (active) {
        if (hands != hands_now) {
            end_all(animation);
            //The native context-change branch preserves stage12.
            clear_active();
        } else {
            const char* channel = CHANNELS[static_cast<std::size_t>(hands)];
            if (!animation.channels.in_transition(channel) && animation.channels.remaining(channel) < TENTH) {
                if (stage == 0) {
                    auto r = next_stage(animation, input, 1);
                    if (r.is_err()) return rs::make_err(r.unwrap_err());
                } else if (stage == 1 && !held_at(direction)) {
                    auto r = next_stage(animation, input, 2);
                    if (r.is_err()) return rs::make_err(r.unwrap_err());
                } else if (stage == 2) {
                    stage = -1;
                    clear_active();
                }
            }
        }
    }
    if (!active) return std::optional<GesturePublication>();
    auto gesture = selection(direction, input.selections);
    if (gesture.is_err()) return rs::make_err(gesture.unwrap_err());
    animation.set_attribute(
        SettableAttribute{encode("GstrDistToCog"), input.state_offboard75 ? 1.0f : input.distance_to_cog, false, -1});
    return std::optional<GesturePublication>(GesturePublication{gesture.unwrap(), direction == 3});
}

rs::Result<rs::Unit, std::string> CharacterGesture::next_stage(MotionAnimation& animation,
                                                               const CharacterGestureInputs& input,
                                                               std::int32_t next) {
    auto name = animation_name(direction, hands, next, input.selections);
    if (name.is_err()) return rs::make_err(name.unwrap_err());
    stage = next;
    //82BAAED8 -> channel virtual20=82D1D030 chooses embedded Sequence,
    //vtable8231E1F8. It waits for the old clip's actual end; it is not a
    //zero-duration blend. The helper enables resurrection and creation.
    TransitionSettings transition;
    transition.kind = 4;
    transition.seconds = 0.0f;
    transition.under = 0;
    transition.matching = 0;
    transition.use_channels_from_weights = false;
    auto r = animation.transition_channel(CHANNELS[static_cast<std::size_t>(hands)], name.unwrap(),
                                          settings(next == 1, false), transition, true, true);
    if (r.is_err()) return rs::make_err(r.unwrap_err());
    return rs::Unit{};
}

void CharacterGesture::clear_active() {
    active = false;
    direction = -1;
    hands = -1;
}

}  // namespace s3::game::graph_host::motion_character_gesture
