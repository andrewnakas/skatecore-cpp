// Port of skate-game/src/graph_host/motion_spin.rs (module `motion_spin`).
#include "s3/game/graph_host/motion_spin.hpp"

#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_spin {

using s3::animation::channel_playback::ChannelSettings;

namespace {
constexpr const char* FRONT = "IA_BODYSPIN_OLLIE_FS_0_N";
constexpr const char* BACK = "IA_BODYSPIN_OLLIE_BS_0_N";

float clamp(float v, float low, float high) {
    v = low - v >= 0.0f ? low : v;
    return high - v >= 0.0f ? v : high;
}

void update_influence(float& value, float& delta, float target, float blend, const Settings& s) {
    //82BABC68..BD9C: first clamp acceleration, then change, then [0,1].
    const float desired = rs::mul_add(blend, target, (1.0f - blend) * value) - value;
    delta = clamp(desired, delta - s.maximum_acceleration, delta + s.maximum_acceleration);
    const float next = clamp(value + delta, value - s.maximum_delta, value + s.maximum_delta);
    value = clamp(next, 0.0f, 1.0f);
}
}  // namespace

rs::Result<Settings, std::string> Settings::load(const s3::data::Collections& data) {
    //Global304 writer8289F9C0..F9EC:DCBADE97CA665643=body_spin.
    auto f = [&](const char* name) { return motion_collections::float_(data, "anim_motion", "body_spin", name); };
    S3_MOTION_TRY(const auto w, motion_collections::words<16>(data, "anim_motion", "body_spin", "spin_map"));
    S3_MOTION_TRY(const auto d,
                  motion_collections::words<16>(data, "animation", "default", "LandingDistanceScalarVsNormalY"));
    Settings s;
    s.map = motion_collections::graph<8>(w, 0, 8);
    S3_MOTION_TRY(s.blend_out, f("spin_influence_blendout"));
    S3_MOTION_TRY(s.blend_in, f("spin_influence_blendin"));
    S3_MOTION_TRY(s.maximum_acceleration, f("spin_clamp_influence_deltadelta"));
    S3_MOTION_TRY(s.maximum_delta, f("spin_clamp_influence_delta"));
    //Global336=anim_motion/inair_disttocog;328=body_tilt.
    S3_MOTION_TRY(s.final_height,
                  motion_collections::float_(data, "anim_motion", "inair_disttocog", "preland_final_disttocom"));
    S3_MOTION_TRY(s.height_velocity,
                  motion_collections::float_(data, "anim_motion", "inair_disttocog", "preland_disttocom_vel"));
    S3_MOTION_TRY(s.on_deck_height,
                  motion_collections::float_(data, "anim_motion", "inair_disttocog", "landingondeck_disttocog"));
    S3_MOTION_TRY(s.override_x, motion_collections::float_(data, "anim_motion", "body_tilt", "overide_preland_x"));
    S3_MOTION_TRY(s.override_velocity_y,
                  motion_collections::float_(data, "anim_motion", "body_tilt", "overide_preland_velY"));
    s.landing_distance = motion_collections::graph<8>(d, 0, 8);
    return s;
}

bool PrelandingPhysical::override_prelanding(const Settings& s) const {
    return air_444 || (air_normal_144_y <= 0.5f && rs::abs(animation_16_x) > s.override_x &&
                       com_velocity_y > s.override_velocity_y);
}

bool PrelandingPhysical::near_landing(const Settings& s) const {
    if (override_prelanding(s) || com_velocity_y >= 0.0f) return false;
    if (offboard_316) return offboard_time_32 < (s.final_height - s.on_deck_height) / s.height_velocity;
    if (air_437) {
        float time = (s.final_height - animation_height_72) / s.height_velocity;
        time *= s.landing_distance.evaluate(clamp(air_normal_36, 0.0f, 1.0f));
        return air_remaining_184 < time;
    }
    return offboard_319 && offboard_time_32 < rs::from_bits(0x3f99999au);
}

rs::Result<rs::Unit, std::string> State::update(MotionAnimation& animation, std::uint32_t category,
                                                std::uint32_t physical_state, bool doing_trick,
                                                std::array<std::uint32_t, 2> busy_hands, bool mirrored,
                                                std::optional<PrelandingPhysical> prelanding,
                                                const Settings& settings) {
    const bool biped_air = physical_state == 503;
    const bool air = category == 2 || biped_air;
    const bool active = air || doing_trick;
    const bool busy = busy_hands[0] != 0 || busy_hands[1] != 0;
    float spin = animation.motion_intent("BodySpin").value_or(0.0f);
    if (mirrored) spin = -spin;
    const float mapped = settings.map.evaluate(rs::abs(spin));
    const float influence = spin >= 0.0f ? mapped : -mapped;
    bool near = false;
    if (air) {
        if (!prelanding) {
            return rs::make_err(std::string("BodySpin airborne branch requires actual prelanding physical outputs"));
        }
        near = prelanding->near_landing(settings);
    }
    std::uint32_t m = mode;
    switch (m) {
        case 0:
            if (category == 3) m = 4;
            if (active) m = busy ? 4 : 1;
            break;
        case 1:
            if (!active) m = 0;
            if (busy) m = 4;
            if (rs::abs(spin) > rs::from_bits(0x3e99999au)) m = 2;
            break;
        case 2:
            if (!active) m = 0;
            if (busy) m = 4;
            if (air) m = 3;
            break;
        case 3:
            if (busy || (near && biped_air)) m = 4;
            if (!air) m = 0;
            break;
        case 4:
            if (!active && category == 1) m = 0;
            break;
        default: break;
    }
    switch (m) {
        case 0:
        case 4:
            for (const char* name : {FRONT, BACK}) {
                animation.channels.end_with(name, rs::from_bits(0x3e4ccccdu), false);
            }
            break;
        case 1:
            front = 0.0f;
            front_delta = 0.0f;
            back = 0.0f;
            back_delta = 0.0f;
            break;
        case 2:
        case 3: {
            if (m == 2) {
                ChannelSettings channel;
                channel.priority = 0;
                channel.keep_alive = false;
                channel.mirrored = false;
                channel.speed = 1.0f;
                channel.blend_in = rs::from_bits(0x3e4ccccdu);
                channel.hold_during_blend_in = false;
                channel.blend_out = rs::from_bits(0x3e99999au);
                channel.hold_during_blend_out = false;
                channel.use_attributes = false;
                if (spin > rs::from_bits(0x3e99999au)) {
                    if (!animation.channels.has(FRONT) && !near) S3_MOTION_CHECK(animation.new_channel(FRONT, FRONT, channel));
                    animation.channels.end_with(BACK, rs::from_bits(0x3e19999au), false);
                }
                if (spin < rs::from_bits(0xbe99999au)) {
                    if (!animation.channels.has(BACK) && !near) S3_MOTION_CHECK(animation.new_channel(BACK, BACK, channel));
                    animation.channels.end_with(FRONT, rs::from_bits(0x3e19999au), false);
                }
            }
            if (animation.channels.has(FRONT)) {
                const float target = clamp(influence, 0.0f, 1.0f);
                const float blend = target < front ? settings.blend_out : settings.blend_in;
                update_influence(front, front_delta, target, blend, settings);
                animation.channels.influence(FRONT, front);
            }
            if (animation.channels.has(BACK)) {
                const float target = -clamp(influence, -1.0f, 0.0f);
                //The original backside branch intentionally reverses this comparison.
                const float blend = target > back ? settings.blend_out : settings.blend_in;
                update_influence(back, back_delta, target, blend, settings);
                animation.channels.influence(BACK, back);
            }
            break;
        }
        default: break;
    }
    mode = m;
    previous_spin = spin;
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion_spin
