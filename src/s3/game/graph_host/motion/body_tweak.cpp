// Port of skate-game/src/graph_host/motion_offboard/body_tweak.rs (module `motion::body_tweak`).
#include "s3/game/graph_host/motion/body_tweak.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::graph_host::motion::body_tweak {

using s3::animation::channel_playback::ChannelSettings;
using s3::animation::playback::TransitionSettings;
using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::skeleton_input::name::encode;

namespace {
rs::Result<rs::Unit, std::string> set_flag(MotionAnimation& animation, std::uint32_t bit, bool value) {
    if (!animation.skater_animation_flags) {
        return rs::make_err(std::string("OffboardBodyTweakBlend requires SkaterAnim flag owner"));
    }
    std::uint32_t& flags = *animation.skater_animation_flags;
    flags = (flags & ~bit) | (value ? bit : 0u);
    return rs::Unit{};
}

rs::Result<rs::Unit, std::string> transition(MotionAnimation& animation, const Settings& settings, bool held,
                                             bool cycle) {
    const std::string& name = !held ? (!cycle ? settings.nb_into : settings.nb_cycle)
                                    : (!cycle ? settings.br_into : settings.br_cycle);
    //82BBB164/270 call IChannelAnimatablev16=82D1D090. Stack57=false
    //resurrection,5F=true create-if-missing,67=true use animation attributes.
    ChannelSettings channel;
    channel.priority = 0;
    channel.keep_alive = cycle;
    channel.mirrored = false;
    channel.speed = 1.0f;
    channel.blend_in = cycle ? 0.5f : 0.15f;
    channel.hold_during_blend_in = false;
    channel.blend_out = cycle ? 0.3f : 0.0f;
    channel.hold_during_blend_out = false;
    channel.use_attributes = true;
    auto r = animation.transition_channel(CHANNEL, name, channel,
                                          TransitionSettings{1, cycle ? 0.1f : 0.15f, 0, 0, false}, false, true);
    if (r.is_err()) return rs::make_err(r.unwrap_err());
    return rs::Unit{};
}
}  // namespace

Settings Settings::parse(const Attributes& a) {
    auto text = [&](const char* name, const char* fallback) {
        const std::string* v = a.text(name);
        return v ? *v : std::string(fallback);
    };
    return Settings{text("NbCycAnim", "B_OBAIR_BODYTWEAK_NB_CYC"), text("NbIntoAnim", "B_OBAIR_BODYTWEAK_NB_INTO"),
                    text("BrCycAnim", "B_OBAIR_BODYTWEAK_BR_CYC"), text("BrIntoAnim", "B_OBAIR_BODYTWEAK_BR_INTO")};
}

void State::begin() {
    // Raw82BBACC0: zero8,20,21,22. Axes12/16 are only zeroed at allocation.
    ticks = 0;
    into_started = false;
    cycle_started = false;
    released = false;
}

rs::Result<rs::Unit, std::string> State::update(const Settings& settings, MotionAnimation& animation,
                                                Controls& controls, Physical physical) {
    const std::optional<float> x = animation.motion_intent("OB_AirBodyTweakX");
    const std::optional<float> y = animation.motion_intent("OB_AirBodyTweakY");
    const bool request = animation.motion_intent("OB_DoAirBodyTweak").has_value();
    const auto [directed, centered] = gates(x, y, request, physical.scalar_92);
    const bool active = directed || centered;
    //82BBB030 is blt: unordered does not take the near-landing branch.
    if (active && !(physical.time_to_land < 0.1f)) {
        const float remaining = animation.channels.remaining(CHANNEL);
        if (animation.channels.has(CHANNEL) || into_started) {
            if (!(remaining > 0.01f) && !cycle_started) {
                auto r = transition(animation, settings, physical.board_held, true);
                if (r.is_err()) return r;
                cycle_started = true;
            }
        } else {
            auto r = transition(animation, settings, physical.board_held, false);
            if (r.is_err()) return r;
            into_started = true;
        }
        filter(x.value_or(0.0f), y.value_or(0.0f), centered);
        const char* names[] = {"bodytweakx", "bodytweaky"};
        for (std::size_t i = 0; i < 2; ++i) {
            animation.set_attribute(SettableAttribute{encode(names[i]), axes[i], false, -1});
        }
        //82BBB328 ->MGv200/8258FBF0;82BBB33C ->ISkaterAnimv116/82B97228.
        controls.seed_from_air_tweak = true;
        auto r = set_flag(animation, 0x8000u, true);
        if (r.is_err()) return r;
    } else if (active && !(physical.time_to_land >= 0.1f)) {
        //82BBB36C ->ISkaterAnimv112/82B97218: one-frame request bit16.
        auto r = set_flag(animation, 0x10000u, true);
        if (r.is_err()) return r;
    } else if (animation.channels.has(CHANNEL)) {
        const float duration = physical.time_to_land - 0.3f >= 0.0f ? 0.3f : physical.time_to_land;
        animation.channels.end_with(CHANNEL, duration, true);
        controls.seed_from_air_tweak = false;
        auto r = set_flag(animation, 0x8000u, false);
        if (r.is_err()) return r;
    }
    ticks = ticks + 1u;
    return rs::Unit{};
}

std::pair<bool, bool> State::gates(std::optional<float> x_in, std::optional<float> y_in, bool request,
                                   float scalar_92) {
    if (static_cast<std::int32_t>(ticks) > 80) released = true;
    //82BBAED8..EE8 uses signed comparisons, not abs(axis).
    if (!released && x_in && !(*x_in >= 0.1f) && y_in && !(*y_in >= 0.1f)) released = true;
    const float x = x_in.value_or(0.0f);
    const float y = y_in.value_or(0.0f);
    const float squared = rs::mul_add(y, y, x * x);
    //Same independent PC seed as native_arithmetic; both native refinements
    //remain below. This is not a claim of bit-exact Xenon rsqrt emulation.
    float inverse = rs::recip(rs::sqrt(squared));
    for (int i = 0; i < 2; ++i) {
        const float correction = rs::mul_add(-squared, inverse * inverse, 1.0f);
        inverse = rs::mul_add(inverse * 0.5f, correction, inverse);
    }
    const float magnitude = squared == 0.0f ? 0.0f : squared * inverse;
    const bool directed = released && magnitude >= 0.1f;
    //82BBAFD4 is ble. This preserves its unordered path as well.
    return {directed, request || (!(scalar_92 <= 4.0f) && !directed)};
}

void State::filter(float x, float y, bool centered) {
    //Raw82BBB290..2C8: preserve fmuls/fmadds order and axis identity.
    if (centered) {
        axes = {axes[0] * 0.85f, axes[1] * 0.85f};
    } else {
        axes = {rs::mul_add(x, 0.15f, axes[0] * 0.85f), rs::mul_add(axes[1], 0.85f, y * 0.15f)};
    }
}

void State::end(MotionAnimation& animation) {
    //82BBB418 ends the channel from its last frame, but does not clear MGbit20.
    if (animation.channels.has(CHANNEL)) animation.channels.end_with(CHANNEL, 0.1f, true);
}

}  // namespace s3::game::graph_host::motion::body_tweak
