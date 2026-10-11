// Port of skate-game/src/physics/biped_ground/lifecycle.rs
//! Ground Post82D32AA0 and complete typed Fill82D32D38/82D785F8.
//! Parent publishes every returned field through the shared output records.
#include "s3/game/physics/biped_ground/lifecycle.hpp"

#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::biped_ground::lifecycle {

/// Apply every represented record field and return state-template writes for
/// the parent's common publication reset/reconstruction.
/// The actual shared record type is PhysicalPlayerInput, not PhysicsOutputFields.
StatePublication publish_fields(Publication p, ::s3::player::input_phase::PhysicalPlayerInput& physical) {
    auto& o = physical.off_board;
    o.flag_304 = static_cast<std::uint8_t>(p.offboard_flag_304);
    o.kind_88 = p.offboard_kind_88;
    o.scalar_112 = p.offboard_scalar_112;
    o.flag_329 = static_cast<std::uint8_t>(p.offboard_flag_329);
    o.flag_330 = static_cast<std::uint8_t>(p.offboard_flag_330);
    o.distance_116 = p.offboard_distance_116;
    o.flag_334 = static_cast<std::uint8_t>(p.offboard_flag_334);
    o.scalar_32 = p.offboard_scalar_32;
    o.flag_328 = static_cast<std::uint8_t>(p.offboard_flag_328);
    o.flags_306_307 = {static_cast<std::uint8_t>(p.offboard_hand_flags_306_307[0]),
                       static_cast<std::uint8_t>(p.offboard_hand_flags_306_307[1])};
    physical.reckoning.vector_144 = rs::to_bits(p.animation_vector_144);
    physical.reckoning.flag_164 = static_cast<std::uint8_t>(p.animation_flag_164);
    physical.state.counter_36 = p.physics_counter_36;
    physical.state.skitch_value_40 = p.physics_counter_40;
    return StatePublication{p.physics_counter_36, p.physics_counter_40, p.physics_flag_86};
}

rs::Result<CollisionSettings, std::string> load_collision_settings(const sd::Collections& data) {
    auto value = [&](const char* name) { return data.float_("physics_wipeout", "default", name); };
    CollisionSettings s{};
    S3G_TRY(s.vehicle_scalar, value("Wipeout_OB_VehicleScalar"));
    S3G_TRY(s.vehicle_contact, value("Wipeout_OB_VehicleContact"));
    S3G_TRY(s.maximum_displacement, value("Wipeout_OB_SkeletonMaxDisp"));
    S3G_TRY(s.maximum_arm_contact, value("Wipeout_OB_SkeletonMaxContactArms"));
    S3G_TRY(s.maximum_body_contact, value("Wipeout_OB_SkeletonMaxContact"));
    S3G_TRY(s.minimum_speed, value("Wipeout_OB_MinSpeed"));
    S3G_TRY(s.maximum_squash, value("Wipeout_OB_MaxSquash"));
    S3G_TRY(s.special_scalar, value("Hash_472174920C68FBE3"));
    return s;
}

/// Called AFTER skeleton feedback, before shared FootIK Post consumes requests.
/// `collision` borrows the completed canonical Frame and real Skeleton16336 /
/// SkeletonCollision4072/4056; there is no pre-solve/default observation path.
void post(SkaterRuntime& skater, CollisionInput collision) {
    const auto& p = skater.player_input.processed;
    auto& owner = skater.biped_ground;
    core::PostInput input{};
    input.collision = collision;
    input.processed_flags_2484 = p.flags_2484;
    input.processed_velocity_608 = rs::from_bits(p.vectors_544_560_592_608[3]);
    input.skeleton_displacement_16288 = skater.collision_extra_errors[0];
    input.skeleton_displacement_16304 = skater.collision_extra_errors[1];
    input.ground_kind_356 = owner.contact.kind_164;
    core::post_physics(owner.ground, skater.wipeout.state, owner.collision_settings, input);
}

/// No output is silently dropped: the full packet includes State36/40/86,
/// OffBoard88/112/116/304/306/307/328/329/330/334, and Reckoning144/164.
/// Borrow the ONE state56 FeetIK owner (its historical type calls feet hands).
rs::Result<Publication, std::string> fill(const SkaterRuntime& skater, const FeetState& feet) {
    const auto& owner = skater.biped_ground;
    if (!owner.result) return rs::make_err(std::string("Ground Fill requires completed Ground motion"));
    const auto motion = *owner.result;
    if (!owner.geometry_adjustment) {
        return rs::make_err(std::string("Ground Fill requires this tick's real geometry consumption"));
    }
    const auto geometry = *owner.geometry_adjustment;
    const auto position = geometry.frame_768.position;
    core::PublicationInput input{};
    input.ground_flags_752_to_754 = {geometry.state_752, geometry.state_753, geometry.state_754};
    input.contact_flags_368 = owner.contact.flags_176;
    input.contact_position_192 = owner.contact.position;
    input.query_position_816 = {position.x, position.y, position.z, 0.0f};
    input.processed_flags_2476 = skater.player_input.processed.flags_2476;
    input.ground_kind_356 = owner.contact.kind_164;
    input.ground_scalar_360 = owner.contact.distance_168;
    input.motion_vector_1040 = motion.velocity;
    input.motion_up_864 = motion.physical_frame[1];
    for (std::size_t i = 0; i < 2; ++i) {
        const auto& foot = feet.hands[i];
        input.hand_flags[i] = {foot.flags_104_to_107[1], foot.flags_104_to_107[2], foot.flags_104_to_107[3]};
    }
    return core::publish(owner.ground, input);
}

}  // namespace s3::game::physics::biped_ground::lifecycle
