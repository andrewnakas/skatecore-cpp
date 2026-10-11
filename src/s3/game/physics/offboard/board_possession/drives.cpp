// Port of skate-game/src/physics/offboard/board_possession/drives.rs
#include "s3/game/physics/offboard/board_possession/drives.hpp"

#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_parameters.hpp"
#include "s3/core/physics/drive_solver/builder.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::game::physics::offboard::board_possession::drives {

using sc::physics::drive_frames::RetailDriveFrame;
using sc::physics::drive_frames::RetailDriveFrames;
using sc::physics::drive_parameters::RetailDriveDynamics;
using sc::physics::drive_parameters::RetailDriveParams;
using sc::physics::drive_parameters::RetailDriveType;

void append(const sc::player::offboard::board_possession::State& state, BodySnapshot deck,
            std::array<BodySnapshot, 2> hands, std::size_t deck_reaction, std::array<std::size_t, 2> hand_reactions,
            float dt, std::vector<RetailDriveRows>& rows) {
    for (std::size_t i = 0; i < 2; ++i) {
        if (((deck.state_flags | hands[i].state_flags) & 4u) == 0) continue;
        const auto drive = state.hands[i];
        auto frame = [](const Frame& f) {
            const auto v = affine(f);
            return RetailDriveFrame{sc::physics::drive_frames::retail_quaternion_from_basis(v.basis), v.translation};
        };
        auto params = [](const std::array<std::uint32_t, 4>& v) {
            return RetailDriveParams{rs::from_bits(v[0]), rs::from_bits(v[1]), rs::from_bits(v[2]),
                                     RetailDriveType::HardDrive};
        };
        const RetailDriveBodyState body_a = body(hands[i], hand_reactions[i]);
        const RetailDriveBodyState body_b = body(deck, deck_reaction);
        //82763BC4..3C44 normalizes both quaternions of every registered
        //drive before Jacobian construction, including possession drives.
        const RetailDriveFrames frames =
            sc::physics::skeleton_body::prepare_bone_drive_frames(RetailDriveFrames{frame(drive.child), frame(drive.parent)});
        rows.push_back(sc::physics::drive_solver::build_drive_rows(
            body_a, body_b, frames, RetailDriveDynamics{params(drive.dynamics[0]), params(drive.dynamics[1])}, dt));
    }
}

RetailDriveBodyState body(BodySnapshot b, std::size_t reaction_index) {
    const auto r = b.rates;
    RetailDriveBodyState out{};
    out.reaction_index = reaction_index;
    out.state = b.state_flags;
    out.orientation = r.orientation;
    out.basis = r.basis;
    out.center_of_mass = r.position;
    out.linear_velocity = r.linear_velocity;
    out.angular_velocity = r.angular_velocity;
    out.force_acceleration = r.force_acceleration;
    out.torque_acceleration = r.torque_acceleration;
    out.inverse_mass = b.inertia.inverse_mass;
    out.world_inverse_inertia = sc::physics::rigid_body::pack_world_inverse_inertia(r.world_inverse_inertia);
    return out;
}

}  // namespace s3::game::physics::offboard::board_possession::drives
