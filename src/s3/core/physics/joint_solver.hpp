// Port of skate-core/src/physics/joint_solver.rs.
//
// Semantic port of Skate 3 TU3's six-degree joint iteration.
//
// Skate 3 builds one 384-byte `JointJacobian` for each of the six skateboard
// assembly joints. `rw::physics::Simulation::Solve` visits contacts, joints,
// and drives in that order on each configured outer iteration. The joint loop
// starts near `0x82AE2BC8`; the loop near `0x82AE2E78` is the drive family.
//
// Generated-code validation is withdrawn. All arithmetic and packed-lane
// semantics require independent review against TU3 IDA before parity claims.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::joint_solver {

using rigid_body::RetailReactionCorrections;

namespace tu3 {
inline constexpr std::uint32_t SIMULATION_SOLVE = 0x82AE27D0u;
inline constexpr std::uint32_t JOINT_SOLVE_LOOP = 0x82AE2BC8u;
inline constexpr std::uint32_t DRIVE_SOLVE_LOOP = 0x82AE2E78u;
inline constexpr std::uint32_t JOINT_BATCH_BUILD = 0x82AE39D0u;
inline constexpr std::uint32_t JOINT_JACOBIAN_BUILD = 0x82AE3BC8u;
inline constexpr std::size_t JACOBIAN_BYTES = 384;
inline constexpr std::size_t REACTION_BYTES = 64;
}  // namespace tu3

inline constexpr std::size_t JACOBIAN_VECTOR_COUNT = tu3::JACOBIAN_BYTES / 16;
inline constexpr std::size_t REACTION_VECTOR_COUNT = tu3::REACTION_BYTES / 16;

/// Raw guest word order of TU3's 384-byte `JointJacobian`.
struct RetailJointJacobian {
    std::array<std::uint32_t, JACOBIAN_VECTOR_COUNT * 4> words{};
};
S3_DERIVE_EQ(RetailJointJacobian, words)

/// Four raw vectors indexed by a rigid body's solver id.
struct RetailJointReactionBlock {
    std::array<std::uint32_t, REACTION_VECTOR_COUNT * 4> words{};

    /// Packs the two reaction vectors consumed by the joint-family branch.
    ///
    /// TU3 leaves reaction vectors one and three to the contact family's
    /// position/orientation correction.
    static RetailJointReactionBlock from_reactions(RetailReactionCorrections reactions) {
        RetailJointReactionBlock block;
        block.words[0] = rs::to_bits(reactions.linear_displacement.x);
        block.words[1] = rs::to_bits(reactions.linear_displacement.y);
        block.words[2] = rs::to_bits(reactions.linear_displacement.z);
        block.words[8] = rs::to_bits(reactions.angular_displacement.x);
        block.words[9] = rs::to_bits(reactions.angular_displacement.y);
        block.words[10] = rs::to_bits(reactions.angular_displacement.z);
        return block;
    }

    /// Writes back only the two joint-owned velocity correction vectors.
    void write_velocity_reactions(RetailReactionCorrections& reactions) const {
        reactions.linear_displacement.x = rs::from_bits(words[0]);
        reactions.linear_displacement.y = rs::from_bits(words[1]);
        reactions.linear_displacement.z = rs::from_bits(words[2]);
        reactions.angular_displacement.x = rs::from_bits(words[8]);
        reactions.angular_displacement.y = rs::from_bits(words[9]);
        reactions.angular_displacement.z = rs::from_bits(words[10]);
    }
};
S3_DERIVE_EQ(RetailJointReactionBlock, words)

/// One joint pass using TU3's fused vector operations and projector.
void solve_joint_iteration(RetailJointJacobian& jacobian, RetailJointReactionBlock& reaction_a,
                           RetailJointReactionBlock& reaction_b);

}  // namespace s3::physics::joint_solver
