// Port of skate-game/src/physics/ground_runtime/settings.rs.
#include "s3/game/physics/ground_runtime/settings.hpp"

#include <cerrno>
#include <cstdlib>

#include "s3/game/difficulty.hpp"
#include "s3/game/physics/ground_runtime/surface.hpp"
#include "s3/game/trainer.hpp"

namespace s3::game::physics::ground_runtime::settings {

rs::Result<GroundProfiles, std::string> GroundProfiles::load(const sd::Collections& data) {
    GroundProfiles out;
    for (const char* mode : difficulty::NATIVE_MODES) {
        std::vector<std::shared_ptr<const GroundSettings>> row;
        for (std::uint32_t surface = 1; surface <= 5; ++surface) {
            S3G_TRY(const char* key, surface::surface_key(surface));
            S3G_TRY(auto settings, GroundSettings::load(data, mode, key));
            row.push_back(std::make_shared<const GroundSettings>(std::move(settings)));
        }
        out._0.push_back(std::move(row));
    }
    return out;
}

rs::Result<std::shared_ptr<const GroundSettings>, std::string> GroundProfiles::select(std::uint32_t mode,
                                                                                      std::uint32_t surface) const {
    if (surface >= 1 && mode < _0.size()) {
        const auto& row = _0[mode];
        const std::uint32_t s = surface - 1;
        if (s < row.size()) return row[s];
    }
    return rs::make_err(fmt::cat("Invalid processed physics mode/surface ", mode, "/", surface));
}

rs::Result<GroundSettings, std::string> GroundSettings::load(const sd::Collections& data, std::string_view mode_,
                                                             std::string_view surface_) {
    const std::string mode(mode_);
    const std::string surface(surface_);
    auto f = [&](const char* class_, const char* field) { return data.float_(class_, "default", field); };
    auto m = [&](const char* field) { return data.float_("physics_mode", mode, field); };
    auto s = [&](const char* field) { return data.float_("physics_surfaces", surface, field); };
    auto c8 = [&](const char* class_, const char* field) { return curve8(data, class_, "default", field); };
    auto c16 = [&](const char* class_, const char* field) { return curve16(data, class_, "default", field); };
    //82F826F8 broadcasts82181A88 into830BD350.82F82610 similarly
    //broadcasts82165A10 (zero) into830BD380 for speed override direction.
    const float t = rs::from_bits(0x358637bdu);
    const std::array<float, 4> threshold{t, t, t, t};
    GroundSettings o;
    o.push_target_multiplier = 1.0f;
    S3G_TRY(o.foot_force_offset, f("physics_feet", "FootForceOffset"));
    S3G_TRY(o.absorption_front, f("physics_feet", "AbsorptionFootForceScalar"));
    S3G_TRY(o.absorption_rear, f("physics_feet", "AbsorptionFootForceRearScalar"));
    S3G_TRY(o.surface_braking_factor, s("BrakingScalar"));
    // steering
    S3G_TRY(o.steering.hard_turn_increase, f("physics_steering", "HardTurnIncrease"));
    S3G_TRY(o.steering.damping, f("physics_steering", "Damping"));
    S3G_TRY(o.steering.speed_graph_max_speed, f("physics_steering", "SteeringVsSpeedMaxSpeed"));
    S3G_TRY(o.steering.push_scalar_increment, f("physics_steering", "PushScalarIncriment"));
    S3G_TRY(o.steering.push_scalar_decrement, f("physics_steering", "PushScalarDecriment"));
    S3G_TRY(o.steering.push_scalar_min, f("physics_steering", "PushScalarMin"));
    S3G_TRY(o.steering.manual_scalar, f("physics_manual", "Tilt_TiltScalar"));
    S3G_TRY(o.steering.general_scalar, f("physics_steering", "GeneralScalar"));
    S3G_TRY(o.steering.tight_trucks_scalar, f("physics_steering", "SteeringScalar_TightTrucks"));
    S3G_TRY(o.steering.speed_graph, c8("physics_steering", "SteeringVsSpeed"));
    S3G_TRY(o.steering.input_graph, c8("physics_steering", "SteeringVsInput"));
    S3G_TRY(o.steering.tilt_blending, f("physics_steering", "SteeringTiltBlending"));
    // wobble
    S3G_TRY(o.wobble.frequency_time, c8("physics_speed_wobble", "FrequencyVsTime"));
    S3G_TRY(o.wobble.frequency_speed, c8("physics_speed_wobble", "FrequencyVsSpeed"));
    S3G_TRY(o.wobble.amplitude_time, c8("physics_speed_wobble", "AmplitudeVsTime"));
    S3G_TRY(o.wobble.amplitude_speed, c8("physics_speed_wobble", "AmplitudeVsSpeed"));
    S3G_TRY(o.wobble.tightness_threshold, f("physics_speed_wobble", "TruckTightnessSpeedMaxAdjust"));
    S3G_TRY(o.wobble.time_range, f("physics_speed_wobble", "TimeMax"));
    S3G_TRY(o.wobble.speed_range, f("physics_speed_wobble", "SpeedRange"));
    S3G_TRY(o.wobble.frequency_scale, f("physics_speed_wobble", "MaxFrequency"));
    S3G_TRY(o.wobble.amplitude_scale, f("physics_speed_wobble", "MaxAmplitude"));
    S3G_TRY(o.wobble.crouch_threshold, f("physics_speed_wobble", "CrouchStartSpeedMaxAdjust"));
    S3G_TRY(o.wobble.height_min, f("physics_speed_wobble", "CrouchMin"));
    S3G_TRY(o.wobble.height_max, f("physics_speed_wobble", "CrouchMax"));
    // propulsion
    S3G_TRY(o.propulsion.braking.input_force, f("physics_brakes", "FootBrakeForce"));
    S3G_TRY(o.propulsion.braking.override_force, f("physics_brakes", "TailBrakeForce"));
    S3G_TRY(o.propulsion.braking.minimum_speed, f("physics_brakes", "SlowSpeed"));
    S3G_TRY(o.propulsion.maximum_pushable_speed, f("physics_push", "MaxPushableSpeed"));
    S3G_TRY(o.propulsion.mode_speed_changes[0], m("MaxPushDVStart"));
    S3G_TRY(o.propulsion.mode_speed_changes[1], m("MaxPushDVEnd"));
    // force
    S3G_TRY(o.force.range_1220, f("physics_feet", "LandingForceTime"));
    S3G_TRY(o.force.speed_scale_1224, f("physics_feet", "MinForce"));
    S3G_TRY(o.force.scale_1228, f("physics_feet", "MaxLandingForce"));
    o.force.normal_threshold = threshold;
    // speed
    S3G_TRY(o.speed.negative_gain, f("physics_speed_conservation", "NegativeGeneralAmount"));
    S3G_TRY(o.speed.maximum_gravity_acceleration, f("physics_speed_conservation", "MaxGravityAcceleration"));
    S3G_TRY(o.speed.gravity, f("physics_speed_conservation", "Gravity"));
    S3G_TRY(o.speed.positive_gain, f("physics_speed_conservation", "GeneralAmount"));
    S3G_TRY(o.speed.coffin_acceleration, f("physics_speed_conservation", "CoffinAcceleration"));
    S3G_TRY(o.speed.speed_error_bound, s("Friction_MaxSpeedChange"));
    S3G_TRY(o.speed.surface_friction, curve8(data, "physics_surfaces", surface, "FrictionVsSpeedNew"));
    S3G_TRY(o.speed.manual_acceleration, f("physics_manual", "SpinCorrectiveForce"));
    S3G_TRY(o.speed.negative_manual_angle_limit, f("physics_manual", "MinSpeedForCorrection"));
    S3G_TRY(o.speed.manual_angle_limit, f("physics_manual", "MaxSpeedForCorrection"));
    S3G_TRY(o.speed.no_input_delay, f("physics_friction", "NoInputTime"));
    S3G_TRY(o.speed.no_input_friction, c8("physics_friction", "FrictionVsSpeed_NoInput"));
    S3G_TRY(o.speed.manual_friction, c8("physics_friction", "FrictionVsSpeed_Manual"));
    S3G_TRY(o.speed.override_enabled, data.boolean("physics_mode", mode, "MotorEnabled"));
    S3G_TRY(o.speed.override_speed, m("MotorTopSpeed"));
    o.speed.normal_threshold = threshold;
    o.speed.override_direction_threshold = {0.0f, 0.0f, 0.0f, 0.0f};
    // slide
    S3G_TRY(o.slide.angle_response, c16("physics_friction", "SlideVsNormal"));
    S3G_TRY(o.slide.speed_response, c8("physics_friction", "SlideVsSpeed"));
    S3G_TRY(o.slide.time_response, c16("physics_heading", "SideFrictionFactor"));
    S3G_TRY(o.slide.scalar_516, f("physicswheels", "SoftestWheelSideFrictionScalar"));
    S3G_TRY(o.slide.heading_time_limit, s("StraightenOut_ForceTime"));
    S3G_TRY(o.slide.friction, s("SideFriction_Scalar"));
    // straighten
    S3G_TRY(o.straighten.time_response, c16("physics_heading", "StraightenOutForce"));
    S3G_TRY(o.straighten.opposite_turn_limit, f("physics_heading", "StraightenMaxTurnInput"));
    //Lookup8 DF474D0A659A7AE3, direct82D94F80..FC0 binding.
    S3G_TRY(o.straighten.time_scalar, f("physicswheels", "SoftestWheelStraightenOutTimeScalar"));
    S3G_TRY(o.straighten.heading_time_limit, s("StraightenOut_ForceTime"));
    S3G_TRY(o.straighten.strength, s("StraightenOut_Scalar"));
    // heading
    S3G_TRY(o.heading.manual_wrong_wheel_scalar, f("physics_manual", "SpinNoLiftScalar"));
    S3G_TRY(o.heading.manual_damping, f("physics_manual", "ManualSpinBlendSpeed"));
    S3G_TRY(o.heading.speed_max, f("physics_heading", "HeadingAdjustMaxSpeed"));
    S3G_TRY(o.heading.heading_strength, f("physics_heading", "HeadingAdjustFactor"));
    S3G_TRY(o.heading.turn_strength, f("physics_heading", "TurnTorqueScalar"));
    S3G_TRY(o.heading.angular_response, c8("physics_heading", "TurnTorqueVsNegAngVel"));
    S3G_TRY(o.heading.manual_response, c8("physics_manual", "SpinVsSpeed"));
    S3G_TRY(o.heading.inclination_response, c8("physics_heading", "HeadingAdjustVsSlope"));
    S3G_TRY(o.heading.speed_response, c8("physics_heading", "HeadingAdjustVsSpeed"));
    o.heading.normal_threshold = threshold;
    // anti_flip
    S3G_TRY(o.anti_flip.axis_96_response, c8("physics_feet", "AntiFlipTorqueX"));
    S3G_TRY(o.anti_flip.axis_64_response, c8("physics_feet", "AntiFlipTorqueZ"));
    o.anti_flip.normal_threshold = threshold;
    S3G_TRY(o.manual, manual_settings(data));
    S3G_TRY(o.manual_mode.correction_angular_speed_threshold, m("Hash_84CB2F459F3FC811"));
    S3G_TRY(o.manual_mode.corrective_force_enabled, data.boolean("physics_mode", mode, "Hash_F8CBC0F5FEF2240E"));
    // drag
    S3G_TRY(o.drag.brake_speed, f("physics_brakes", "SlowSpeed"));
    S3G_TRY(o.drag.balance_speed, f("physics_brakes", "ManualSpinDragSpeed"));
    S3G_TRY(o.drag.comparison_threshold, f("physics_brakes", "ManualSpinDragNormal"));
    S3G_TRY(o.drag.balance_drag, f("physics_brakes", "ManualSpinDrag"));
    S3G_TRY(o.collision_duration, f("physics_collision", "CollisionTorqueMaxTime"));
    S3G_TRY(o.collision_scale, f("physics_collision", "CollisionTorqueFadeoff"));
    S3G_TRY(o.contact_force_time, f("physics_feet", "MaxTimeWallRidingForFootForce"));
    {
        S3G_TRY(const float static_friction, s("WheelStaticFriction"));
        o.wheel_material.static_friction = static_friction * wheel_grip_scale();
        S3G_TRY(const float dynamic_friction, s("WheelDynamicFriction"));
        o.wheel_material.dynamic_friction = dynamic_friction * wheel_grip_scale();
        S3G_TRY(o.wheel_material.restitution, f("physicswheels", "WheelRestitution"));
    }
    S3G_TRY(o.wobble_activation, m("Hash_77AFCE78FE1206CA"));
    S3G_TRY(o.wobble_amplitude, m("Hash_5B57F2CCCCEEF430"));
    return o;
}

GroundSettings GroundSettings::tuned(const trainer::TrainerTuning& tuning) const {
    GroundSettings result = *this;
    result.push_target_multiplier = tuning.push_speed;
    result.propulsion.maximum_pushable_speed *= tuning.push_speed;
    for (auto& dv : result.propulsion.mode_speed_changes) {
        dv *= tuning.push_power;
    }
    result.propulsion.braking.input_force *= tuning.braking;
    result.propulsion.braking.override_force *= tuning.braking;
    result.steering.general_scalar *= tuning.steering;
    result.wobble_amplitude *= tuning.wobble;
    result.slide.friction *= tuning.grip;
    result.wheel_material.static_friction *= tuning.grip;
    result.wheel_material.dynamic_friction *= tuning.grip;
    result.heading.turn_strength *= tuning.turn_power;
    result.drag.balance_drag *= tuning.manual_drag;
    return result;
}

GroundBoardSettings GroundSettings::board() const {
    return GroundBoardSettings{
        steering,
        wobble,
        propulsion,
        force,
        speed,
        slide,
        straighten,
        heading,
        anti_flip,
        manual,
        manual_mode,
        drag,
        collision_duration,
        collision_scale,
        contact_force_time,
        rs::from_bits(0x37800000u),
    };
}

rs::Result<ManualSettings, std::string> manual_settings(const sd::Collections& d) {
    auto f = [&](const char* field) { return d.float_("physics_manual", "default", field); };
    auto gains = [&](const char* p, const char* i,
                     const char* v) -> rs::Result<sc::physics::manual::settings::ManualGains, std::string> {
        sc::physics::manual::settings::ManualGains g;
        S3G_TRY(g.proportional, f(p));
        S3G_TRY(g.integral, f(i));
        S3G_TRY(g.derivative, f(v));
        return g;
    };
    ManualSettings o;
    S3G_TRY(o.noise_vs_speed, curve8(d, "physics_manual", "default", "NoiseVsSpeed"));
    S3G_TRY(o.torque_scale_without_contact, f("TorqueScalarWithNoContact"));
    S3G_TRY(o.torque_bleed_without_contact, f("TorqueBleedOffWithNoContact"));
    S3G_TRY(o.start_torque_scale, f("StartTorqueScalar"));
    S3G_TRY(o.procedural_noise_scale, f("ProceduralNoiseScalar"));
    S3G_TRY(o.procedural_noise_frequency, f("ProceduralNoiseFreq"));
    S3G_TRY(o.powerslide, gains("PowerScalar_P", "PowerScalar_I", "PowerScalar_D"));
    S3G_TRY(o.manual, gains("ManualScalar_P", "ManualScalar_I", "ManualScalar_D"));
    S3G_TRY(o.maximum_tilt_degrees, f("MaxTiltAngle"));
    S3G_TRY(o.maximum_angle_error, f("MaxAngleDiff"));
    S3G_TRY(o.derivative_limit, f("D_Scalar_Limit"));
    S3G_TRY(o.brake_tilt_degrees, f("BrakeTiltAngle"));
    S3G_TRY(o.animation_noise_scale, f("AnimationNoiseScalar"));
    return o;
}

rs::Result<PointGraph<8>, std::string> curve8(const sd::Collections& d, std::string_view c_, std::string_view k_,
                                              std::string_view f_) {
    const std::string c(c_), k(k_), f(f_);
    S3G_TRY(const sd::Field* field, d.field(c, k, f));
    switch (field->data.size()) {
        case 128: {
            S3G_TRY(auto w, d.words<16>(c, k, f));
            PointGraph<8> out{};
            for (std::size_t i = 0; i < 8; ++i) out.x[i] = rs::from_bits(w[i]);
            for (std::size_t i = 0; i < 8; ++i) out.y[i] = rs::from_bits(w[i + 8]);
            return out;
        }
        case 160: {
            S3G_TRY(auto w, d.words<20>(c, k, f));
            PointGraph<8> out{};
            for (std::size_t i = 0; i < 8; ++i) out.x[i] = rs::from_bits(w[i + 4]);
            for (std::size_t i = 0; i < 8; ++i) out.y[i] = rs::from_bits(w[i + 12]);
            return out;
        }
        default:
            return rs::make_err(fmt::cat("Invalid native eight-point graph ", c, "/", k, "/", f));
    }
}

float wheel_grip_scale() {
    static const float SCALE = [] {
        float scale = 1.0f;
        if (const char* v = std::getenv("SKATE_WHEEL_GRIP")) {
            // Rust `str::parse::<f32>`: the whole string must parse.
            char* end = nullptr;
            errno = 0;
            const float parsed = std::strtof(v, &end);
            if (end != v && *end == '\0' && rs::is_finite(parsed) && parsed > 0.0f) scale = parsed;
        }
        // Say so once: a run whose grip was scaled must never be mistaken for a stock measurement.
        if (scale != 1.0f) {
            log::eprintln(fmt::cat("SKATE_PLAYER_AUDIO wheel_grip_scale ", scale, " (NOT stock -- experiment only)"));
        }
        return scale;
    }();
    return SCALE;
}

rs::Result<PointGraph<16>, std::string> curve16(const sd::Collections& d, std::string_view c, std::string_view k,
                                                std::string_view f) {
    S3G_TRY(auto w, d.words<32>(std::string(c), std::string(k), std::string(f)));
    PointGraph<16> out{};
    for (std::size_t i = 0; i < 16; ++i) out.x[i] = rs::from_bits(w[i]);
    for (std::size_t i = 0; i < 16; ++i) out.y[i] = rs::from_bits(w[i + 16]);
    return out;
}

}  // namespace s3::game::physics::ground_runtime::settings
