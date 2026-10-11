// Port of skate-game/src/physics/wipeout_states/settings.rs
//! Stock vault bindings for WipeoutGround300 and its authored control profiles.
#include "s3/game/physics/wipeout_states/settings.hpp"

namespace s3::game::physics::wipeout_states::settings {

using ::s3::physics::contact::RetailContactMaterial;
using ::s3::player::wipeout_state::profiles::Profile;

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data) {
    auto f = [&](const char* name) { return data.float_("physics_wipeout", "default", name); };
    Settings s{};
    S3G_TRY(s.recovery.minimum_time, f("TeleportMinTimeForAutoReset"));
    S3G_TRY(s.recovery.minimum_settled, f("TeleportMinTimeAfterSettlingForAutoReset"));
    S3G_TRY(s.recovery.maximum_time, f("TeleportMaxTime"));
    S3G_TRY(s.recovery.fade_time, f("TeleportAutoResetFadeOutTime"));
    S3G_TRY(s.recovery.over_speed, f("WipeoutOverSpeed"));
    S3G_TRY(s.recovery.over_minimum_time, f("WipeoutOverMinTime"));
    S3G_TRY(s.remove_target_time, f("TimeToRemoveHook"));
    S3G_TRY(s.remove_drives_time, f("TimeToRemoveDrives"));
    S3G_TRY(s.controlled_weight_step, f("DynamicDriveWeightVelControlled"));
    S3G_TRY(s.collision_weight_step, f("DynamicDriveWeightVelCollision"));
    S3G_TRY(s.board_restitution, f("SkateboardRestitution"));
    S3G_TRY(s.board_friction, f("SkateboardFriction"));
    S3G_TRY(const float drag, data.float_("physicsdeck", "default", "DeckAngularDrag"));
    s.deck_angular_drag = drag * rs::from_bits(0x426FFFFFu);
    S3G_TRY(s.push_force, f("LivingWorldPushForce"));
    S3G_TRY(s.standard_materials[0], material(data, "physicswheels", "Wheel"));
    S3G_TRY(s.standard_materials[1], material(data, "physicstrucks", "Truck"));
    S3G_TRY(s.standard_materials[2], material(data, "physicsdeck", "Deck"));
    return s;
}

rs::Result<RetailContactMaterial, std::string> material(const sd::Collections& data, std::string_view class_,
                                                        std::string_view prefix) {
    const std::string c(class_);
    const std::string p(prefix);
    RetailContactMaterial m{};
    S3G_TRY(m.static_friction, data.float_(c, "default", p + "StaticFriction"));
    S3G_TRY(m.dynamic_friction, data.float_(c, "default", p + "DynamicFriction"));
    S3G_TRY(m.restitution, data.float_(c, "default", p + "Restitution"));
    return m;
}

/// The key is supplied by the original constructor's verified handle binding.
/// PointNegGraph metadata occupies four words before the eight X/Y samples.
rs::Result<Profile, std::string> load_profile(const sd::Collections& data, std::string_view key_) {
    const std::string class_ = "physics_wipeout_control";
    const std::string key(key_);
    auto f = [&](const char* name) { return data.float_(class_, key, name); };
    auto b = [&](const char* name) { return data.boolean(class_, key, name); };
    auto v = [&](const char* name) -> rs::Result<std::array<float, 4>, std::string> {
        S3G_TRY(const auto words, data.words<4>(class_, key, name));
        return rs::from_bits(words);
    };
    S3G_TRY(const auto graph, data.words<20>(class_, key, "SpinVsTime"));
    Profile p{};
    S3G_TRY(p.roll_axis, v("RollOnGroundAxis"));
    S3G_TRY(p.horizontal_axis, v("HorizAlignAxis"));
    S3G_TRY(p.align_euler, v("AlignAxisEuler"));
    for (std::size_t i = 0; i < 8; ++i) {
        p.spin_vs_time.x[i] = rs::from_bits(graph[4 + i]);
        p.spin_vs_time.y[i] = rs::from_bits(graph[12 + i]);
    }
    S3G_TRY(p.direction_follow, f("Hash_1BC908CDB3520A8E"));
    S3G_TRY(p.torque_velocity, f("TorqueVelScalar"));
    S3G_TRY(p.torque_distance, f("TorqueDistScalar"));
    S3G_TRY(p.spin_inertia, f("SpinInertia"));
    S3G_TRY(p.roll_on_ground, b("RollOnGround"));
    S3G_TRY(p.ground_roll_torque, f("RollingOnGroundTorqueVelScalar"));
    S3G_TRY(p.sideways_spin, f("Hash_40B2B3C1A87A4D76"));
    S3G_TRY(p.forward_spin, f("Hash_9047531918285FC3"));
    S3G_TRY(p.horizontal_directed, b("Hash_6009FD75F9EDC436"));
    S3G_TRY(p.drift_maximum_speed, f("DriftMaxSpeed"));
    S3G_TRY(p.drift_forward, f("DriftFactorZ"));
    S3G_TRY(p.drift_sideways, f("DriftFactorX"));
    S3G_TRY(p.align_with_velocity, b("AlignWithVel"));
    S3G_TRY(p.align_ground_with_velocity, b("Hash_9626703A9939FE36"));
    S3G_TRY(p.ground_align_torque, f("AlignOnGroundTorqueVelScalar"));
    S3G_TRY(p.tilt_degrees, f("Hash_B65276A8CC7FB760"));
    S3G_TRY(p.drift_drag, f("Hash_7E6B3A99C0A33ABC"));
    return p;
}

}  // namespace s3::game::physics::wipeout_states::settings
