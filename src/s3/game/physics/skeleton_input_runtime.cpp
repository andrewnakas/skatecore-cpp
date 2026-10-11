// Port of skate-game/src/physics/skeleton_input_runtime.rs.
#include "s3/game/physics/skeleton_input_runtime.hpp"

#include <vector>

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/skeleton_landing.hpp"
#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/grind_air_settings.hpp"
#include "s3/game/physics/offboard/pose_adjust.hpp"
#include "s3/game/physics/skeleton_grind_air.hpp"

namespace s3::game::physics::skeleton_input_runtime {

using sc::physics::board::BodyId;
using sc::riding::ground_correction_math::dot_product;

rs::Result<SkeletonInputRuntime, std::string> SkeletonInputRuntime::load(const sd::Collections& data) {
    SkeletonInputRuntime out;
    S3G_TRY(auto settings, grind_air_settings::load(data));
    out.grind_air_settings = settings;
    return out;
}

Status SkeletonInputRuntime::process_data(sc::physics::board_runtime::BoardRuntime& board,
                                          const sc::physics::board_toolkit::BoardToolkit& toolkit,
                                          const sc::player::input_phase::AnimationInputPacket& packet,
                                          sc::player::input_phase::PhysicalPlayerInput& /*physical*/,
                                          ProcessedPhysicsInput& p, SkeletonOwners& owners, SkeletonPoseInput pose,
                                          const CollisionInput& collision) {
    const auto velocity = board.bodies()[sc::physics::board::index(BodyId::Deck)].rates.linear_velocity;
    deck_velocity = {velocity.x, velocity.y, velocity.z, 0.0f};
    //82ADF7B8: wake sleeping state2, preserving bit3, and clear cooldown
    //even for already active parts. Pool/list ownership is our engine's.
    for (auto& body : owners.body.bodies_mut()) {
        if ((body.state_flags & 7u) == 2u) body.state_flags = (body.state_flags & 8u) | 4u;
        body.rates.cool_down = 0;
    }
    auto& input = owners.animation_input;
    input.reset_processed();
    input.fields.flags2468 = p.flags_2468;
    input.fields.flags2472 = p.flags_2472;
    input.fields.flags2476 = p.flags_2476;
    input.fields.flags2484 = p.flags_2484;
    input.fields.flags2488 = p.flags_2488;
    input.extra.flags2480 = p.flags_2480;
    S3G_CHECK(input.process(pose.attributes, pose.globals, packet.publication.timestep, packet.flags_10932,
                            packet.publication.flags_10375_10496_10784[1] != 0, pose.actions));
    publish_attribute_flags(input, p);
    p.spin_input_2672 = input.fields.spin;

    //These authored offset producers are separate from Ground riding.
    //Do not silently reuse a riding pose if a future state enables them.
    const auto reparented_hands = owners.animated.reparented_hand_indices();
    S3G_CHECK(offboard::pose_adjust::update(
        owners.animated, sc::physics::Slice<const Transform>(pose.globals.data(), pose.globals.size()),
        reparented_hands, p));
    if ((p.flags_2476 & 0x100u) == 0) {
        //82D712E0 clears result48/49 first and always writes adjusting290.
        //Inactive Ground leaves retained total offset/angle untouched.
        grind_air_adjusting = false;
        if (grind_air_active) {
            if (!grind_air_settings)
                return rs::make_err(std::string("GrindAirAdjust stock settings were not loaded"));
            const std::vector<NativeMatrix> globals(pose.globals.begin(), pose.globals.end());
            S3G_TRY(grind_air_adjusting, skeleton_grind_air::update(grind_air, *grind_air_settings, toolkit.deck, p,
                                                                    owners.animated, globals));
        }
    }
    const auto up = rs::from_bits(p.vectors_544_560_592_608[0]);
    const auto com_velocity = rs::from_bits(p.vectors_544_560_592_608[3]);
    std::array<float, 4> com_delta{};
    for (std::size_t i = 0; i < 4; ++i)
        com_delta[i] = owners.body.record.centre_of_mass[i] - owners.body.record.positions[0][i];
    sc::physics::skeleton_landing::LandingInput landing;
    landing.filtered_state = p.filtered_state_2524;
    landing.flags_2468 = p.flags_2468;
    landing.flags_2472 = p.flags_2472;
    landing.flags_2476 = p.flags_2476;
    landing.balance = input.fields.balance;
    landing.physical_com_velocity_along_up = dot_product(com_velocity, up);
    landing.physical_com_height = dot_product(com_delta, up);
    //process_pose replaces this with prior animation COM-currentboard.
    landing.animation_com_height = owners.animated.record.centre_of_mass[1];
    std::optional<animated_skeleton::LandingOnBoard> landing_on_board;
    if ((p.flags_2476 & 0x10100u) == 0x100u) {
        landing_on_board = animated_skeleton::LandingOnBoard{
            toolkit.deck, rs::from_bits(p.vectors_720_784_800_816_832_864[5]), p.flags_2480,
            p.off_board_scalar_2832};
    }
    S3G_CHECK(owners.animated.process_pose(pose.globals, landing, packet.publication.timestep, p.flags_2468,
                                           p.flags_2472, landing_on_board));
    drive_frames = owners.animated.record.pose;
    p.board_at_y_delta_2768 = owners.animated.board_at_y_delta;
    p.flags_2472 = (p.flags_2472 & ~(1u << 17)) | (static_cast<std::uint32_t>(collision.contact_4070) << 17);
    if (collision.has_pose_error_4077) p.collision_pose_error_736 = rs::to_bits(collision.pose_error_16272);
    p.animation_com_to_deck_752 = rs::to_bits(owners.animated.record.com_to_deck_world);
    p.animation_com_to_deck_delta_768 = rs::to_bits(owners.animated.record.com_to_deck_world_delta);
    //The scalar fields and ProcessedPhysicsInput share these native words.
    input.fields.flags2468 = p.flags_2468;
    input.fields.flags2472 = p.flags_2472;
    if (reenable_requested) {
        reenable_requested = false;
        owners.ik.enable_feet(true);
        general::restore_board_target_drives(owners.drives);
        //Native16416 is the prediction-supplied flag;16417 heading is
        //distinct and must not be cleared by the request's completion.
        owners.animated.roots.supplied_prediction = std::nullopt;
    }
    return ok();
}

void publish_attribute_flags(const animation_input::AnimationInput& input, ProcessedPhysicsInput& p) {
    // The scalar and extended attribute consumers write into the same
    // ProcessedPhysicsInput flag lanes that the later physics stages read.
    // Publish each lane explicitly at the native boundary. This is publication
    // only. No flags are synthesized, retained, or filtered here.
    const auto& scalar = input.fields;
    p.flags_2468 = scalar.flags2468;
    p.flags_2472 = scalar.flags2472;
    p.flags_2476 = scalar.flags2476;
    p.flags_2484 = scalar.flags2484;
    p.flags_2488 = scalar.flags2488;
    p.flags_2480 = input.extra.flags2480;
}

}  // namespace s3::game::physics::skeleton_input_runtime
