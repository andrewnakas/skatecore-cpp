// Port of skate-game/src/physics/air_reckoning.rs.
//! Production stock settings and live-input adapter for Reckoning82D8DBD8.
#include "s3/game/physics/air_reckoning.hpp"

#include "s3/game/physics/riding_outputs.hpp"

namespace s3::game::physics::air_reckoning {

namespace reckoning = ::s3::air::reckoning;

void AirReckoning::update_plant(RidingOutputs& riding, const ProcessedPhysicsInput& p, std::array<float, 4> up,
                                std::array<float, 4> heading) {
    reckoning::update_plant(riding.reckoning, riding.reckoning_frames, riding.body_spin, state, settings, up, heading,
                            (p.flags_2468 & (1u << 20)) != 0);
}

rs::Result<AirReckoning, std::string> AirReckoning::load(const sd::Collections& data) {
    S3G_TRY(auto loaded, settings::load(data));
    AirReckoning out;
    out.state = AirState::new_();
    out.settings = std::move(loaded.first);
    out.modes = loaded.second;
    return out;
}

PhysicsAirReckoningFields AirReckoning::fields(const RidingOutputs& riding) const {
    const auto up = riding.reckoning.up;
    const auto normal = riding.reckoning.ground_normal;
    PhysicsAirReckoningFields f;
    f.current_landing_normal_1152 = {up.x, up.y, up.z, 0.0f};
    f.collision_reference_normal_1216 = {normal.x, normal.y, normal.z, 0.0f};
    f.body_spin_angle_1568 = state.spin_angle;
    f.body_spin_speed_1572 = state.spin_speed;
    return f;
}

rs::Result<PhysicsAirReckoningFields, std::string> AirReckoning::update(
    RidingOutputs& riding, const ProcessedPhysicsInput& processed, float physical_body_spin,
    std::array<float, 4> landing_normal, float normal_blend, float target_spin, float flip_request) {
    if (std::size_t(processed.state_variant_index_2528) >= modes.size()) {
        return rs::make_err(fmt::cat("Undefined physics mode ", processed.state_variant_index_2528));
    }
    const auto& mode = modes[processed.state_variant_index_2528];
    reckoning::Input input;
    input.landing_normal = landing_normal;
    input.normal_blend = normal_blend;
    input.target_spin = target_spin;
    input.flip_request = flip_request;
    input.com_to_deck = rs::from_bits(processed.animation_com_to_deck_752);
    input.timestep = processed.timestep_2604;
    input.physical_body_spin = physical_body_spin;
    input.grind_adjusted_body_spin = processed.grind_adjusted_body_spin_2644;
    input.additive_spin = (processed.flags_2484 & (1u << 15)) != 0;
    input.direct_spin = (processed.flags_2472 & (1u << 28)) != 0;
    input.reverse_stance = (processed.flags_2468 & (1u << 20)) != 0;
    input.easy_body_spins = mode.easy_body_spins;
    input.perfect_body_flips = mode.perfect_body_flips;
    reckoning::update(riding.reckoning, riding.reckoning_frames, riding.body_spin, state, settings, input);
    return fields(riding);
}

}  // namespace s3::game::physics::air_reckoning
