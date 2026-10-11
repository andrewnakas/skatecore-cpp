// Port of skate-game/src/physics/biped_ground/sync.rs
//! Ground-owned Sync stages. The caller executes returned launch on its ONE
//! persistent AirSelector before frame correction; no speculative transition.
#include "s3/game/physics/biped_ground/sync.hpp"

#include "s3/game/physics/biped_ground.hpp"

namespace s3::game::physics::biped_ground {

///82D32338. This writes state146 on every call, including the no-launch path.
rs::Result<std::optional<::s3::player::offboard::air_launch::Packet>, std::string> Owner::prepare_air(
    const sync::LaunchInput& input) {
    namespace air_launch = ::s3::player::offboard::air_launch;
    S3G_TRY_OPT(const auto motion, result, "Ground Sync requires completed Ground job");
    const auto& p = input.processed;
    const std::uint32_t flags = contact.flags_176;
    bool launch = (flags & 1u) == 0 && input.elapsed_2664 > rs::from_bits(0x3d4ccccdu);
    launch |= motion.alternate;
    if ((flags & 8u) == 0 && (p.flags_2480 & 0x20180u) == 0 && !ground.flags_144_to_150[6]) {
        launch |= (p.flags_2476 & 0x80000u) != 0;
    }
    ground.flags_144_to_150[2] = launch;
    if (!launch) {
        return std::optional<air_launch::Packet>{};
    }
    //104 is uninitialized by82D2DB98 and overwritten on every produce path.
    auto packet = air_launch::Packet::initialized(0.0f);
    auto produced = air_launch::produce(packet, controller.state, controller.settings.movement_velocity.turn_vs_speed,
                                        this->air_launch, p, false);
    if (produced.is_err()) return rs::make_err(std::string(produced.unwrap_err()));
    const auto frame = motion.animation_frame;
    const auto point = input.skeleton_point_10960;
    for (std::size_t i = 0; i < 4; ++i) {
        packet.position_32[i] = rs::mul_add(
            frame[2][i], point[2], rs::mul_add(frame[1][i], point[1], rs::mul_add(frame[0][i], point[0], frame[3][i])));
    }
    return std::optional<air_launch::Packet>(packet);
}

}  // namespace s3::game::physics::biped_ground
