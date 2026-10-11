// Port of skate-core/src/physics/tests/joint_solver.rs.
#include "harness.hpp"
#include "s3/core/physics/joint_solver.hpp"

namespace {
using namespace s3::physics::joint_solver;
using s3::math::Vector3;

S3_TEST(raw_layout_matches_tu3_strides) {
    CHECK_EQ(sizeof(RetailJointJacobian), std::size_t(384));
    CHECK_EQ(sizeof(RetailJointReactionBlock), std::size_t(64));
}

S3_TEST(reaction_bridge_preserves_contact_only_position_corrections) {
    RetailReactionCorrections reactions{
        Vector3{1.0f, 2.0f, 3.0f},
        Vector3{4.0f, 5.0f, 6.0f},
        Vector3{7.0f, 8.0f, 9.0f},
        Vector3{10.0f, 11.0f, 12.0f},
    };
    auto block = RetailJointReactionBlock::from_reactions(reactions);
    block.words[0] = s3::rs::to_bits(13.0f);
    block.words[9] = s3::rs::to_bits(14.0f);
    block.write_velocity_reactions(reactions);
    CHECK_EQ(reactions.linear_displacement, (Vector3{13.0f, 2.0f, 3.0f}));
    CHECK_EQ(reactions.angular_displacement, (Vector3{7.0f, 14.0f, 9.0f}));
    CHECK_EQ(reactions.position_displacement, (Vector3{4.0f, 5.0f, 6.0f}));
    CHECK_EQ(reactions.orientation_displacement, (Vector3{10.0f, 11.0f, 12.0f}));
}
}  // namespace
