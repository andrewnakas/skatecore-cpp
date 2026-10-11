// Port of skate-game/src/physics/ground_runtime/pumping.rs.
#include "s3/game/physics/ground_runtime/pumping.hpp"

#include "s3/core/physics/board_ground.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/core/riding/pumping/controller.hpp"
#include "s3/core/riding/pumping/geometry.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/ground_runtime/settings.hpp"
#include "s3/game/physics/riding_outputs.hpp"

namespace s3::game::physics::ground_runtime::pumping {

namespace controller = sc::riding::pumping::controller;
using sc::riding::ground_correction_math::dot_product;

Status GroundPumping::update_revert(PumpingState& state, const BoardToolkit& toolkit, const RidingOutputs& riding,
                                    const AnimatedSkeleton& skeleton,
                                    const sc::player::input_phase::ProcessedPhysicsInput& p, float balance) const {
    const float angle =
        balance != 0.0f
            ? 0.0f
            : rs::abs(rs::abs(sc::physics::board_ground::angle_between(xyz(toolkit.deck[2]), riding.reckoning.up)) -
                      rs::f32::consts::FRAC_PI_2);
    controller::PumpingSample sample;
    sample.position = toolkit.deck[3];
    sample.normal = v(riding.reckoning.ground_normal);
    sample.com_to_deck_world = skeleton.record.com_to_deck_world;
    sample.deck_angle = angle;
    sample.intentional_pumping = static_cast<std::uint8_t>((p.flags_2472 >> 16) & 1u);
    S3G_TRY(const auto selected, mode(p.state_variant_index_2528));
    sc::riding::pumping::geometry::NativePumpingGeometry geometry;
    (void)controller::update(state, settings, selected.controller, sample, p.timestep_2604, geometry);
    return ok();
}

rs::Result<GroundPumping, std::string> GroundPumping::load(const sd::Collections& data) {
    auto f = [&](const char* field) { return data.float_("physics_pumping", "default", field); };
    auto c = [&](const char* field) { return settings::curve8(data, "physics_pumping", "default", field); };
    auto mode = [&](const char* key) -> rs::Result<GroundPumpingMode, std::string> {
        auto m = [&](const char* field) { return data.float_("physics_mode", key, field); };
        GroundPumpingMode out;
        S3G_TRY(out.controller.maximum_absorption_per_second, m("Hash_9D1AEE3D7D8A4A7"));
        S3G_TRY(out.controller.maximum_acceleration_per_second, m("Hash_D77AFD320B6241C5"));
        S3G_TRY(out.controller.absorption_factor, m("PumpEffectFactorAbsorption"));
        S3G_TRY(out.controller.acceleration_factor, m("PumpEffectFactor"));
        S3G_TRY(out.unintentional_scalar, m("UnintentionalPumpScalar"));
        return out;
    };
    GroundPumping out;
    S3G_TRY(out.settings.pump_vs_speed, c("PumpVsVel"));
    S3G_TRY(out.settings.pump_vs_time, c("PumpVsTime"));
    S3G_TRY(out.settings.min_crouch_vs_ground_angle, c("MinCrouchVsGroundAngle"));
    S3G_TRY(out.settings.compression_vs_ground_angle, c("CompressionVsGroundAngle"));
    S3G_TRY(out.settings.compression_vs_deck_angle, c("CompressionVsDeckAngle"));
    S3G_TRY(out.settings.height_change_damping, f("PumpEffectDamping"));
    S3G_TRY(out.settings.minimum_height_change, f("MinChangeInCOMBeforePumping"));
    S3G_TRY(out.settings.maximum_height_change, f("MaxDeltaHeightAllowedPerFrame"));
    S3G_TRY(out.settings.ground_compression_scale, f("CompressionGroundScalar"));
    S3G_TRY(out.settings.deck_compression_scale, f("CompressionDeckScalar"));
    S3G_TRY(out.settings.angular_speed_damping, f("AngularSpeedDamping"));
    S3G_TRY(out.modes[0], mode("easy"));
    S3G_TRY(out.modes[1], mode("normal"));
    S3G_TRY(out.modes[2], mode("hardcore"));
    S3G_TRY(out.modes[3], mode("motorized"));
    S3G_TRY(out.modes[4], mode("test"));
    return out;
}

rs::Result<GroundPumpingMode, std::string> GroundPumping::mode(std::uint32_t index) const {
    if (index >= modes.size()) {
        return rs::make_err(fmt::cat("Invalid pumping physics mode ", index));
    }
    return modes[index];
}

void GroundPumping::update(PumpingState& state, const BoardToolkit& toolkit, const RidingOutputs& riding,
                           const AnimatedSkeleton& skeleton, std::uint32_t flags_2476, GroundPumpingMode mode) const {
    const auto up = v(riding.reckoning.up);
    const auto animated_at = skeleton.board_frames.animation_target[2];
    float deck_angle = 0.0f;
    if (rs::abs(dot_product(toolkit.deck[2], up)) > rs::abs(dot_product(animated_at, up))) {
        const auto axis = xyz(riding.reckoning_frames.ground[0]);
        const float deck = wrap(sc::riding::collision_response::signed_angle(xyz(toolkit.deck[2]), xyz(up), axis));
        const float animated = wrap(sc::riding::collision_response::signed_angle(xyz(animated_at), xyz(up), axis));
        deck_angle = rs::abs(deck - animated);
    }
    controller::PumpingSample sample;
    sample.position = toolkit.deck[3];
    sample.normal = v(riding.reckoning.ground_normal);
    sample.com_to_deck_world = skeleton.record.com_to_deck_world;
    sample.deck_angle = deck_angle;
    sample.intentional_pumping = static_cast<std::uint8_t>((flags_2476 >> 1) & 1u);
    sc::riding::pumping::geometry::NativePumpingGeometry geometry;
    (void)controller::update_ground(state, settings, mode.controller, sample, geometry);
}

float wrap(float angle) {
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    const float signed_ = fraction - (fraction > 0.5f ? 1.0f : 0.0f);
    return signed_ * rs::from_bits(0x40c90fdbu);
}
sc::math::Vector3 xyz(std::array<float, 4> v) { return sc::math::Vector3::new_(v[0], v[1], v[2]); }
std::array<float, 4> v(sc::math::Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }

}  // namespace s3::game::physics::ground_runtime::pumping
