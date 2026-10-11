// Port of skate-core/src/physics/board_animation.rs.
// Original TU3 board error capture82C04368 and animation blend82C01878.
// PC arithmetic and the recovered refinements are used; this is not a claim
// of instruction-identical Xenon estimates or exceptional-lane execution.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::physics::board_animation {

using Transform = skeleton_animation_record::AnimationPartTransform;
using point_graph::PointGraph;

struct BoardAnimationSettings {
    PointGraph<8> slow;
    PointGraph<8> fast;
};

///Board240 and288. This state must be shared between physical Ground error
///capture and subsequent animated-board updates; it is not a per-state cache.
struct BoardAnimation {
    //Default: original constructor82C01100/1104.
    std::array<float, 4> rotation_error{0.0f, 0.0f, 0.0f, 1.0f};
    bool blending = false;

    static BoardAnimation default_() { return BoardAnimation{}; }

    ///82C0606C resets the quaternion. The source does not clear byte288.
    void reset();

    ///82C04368: target inverse * actual DECK PART frame, reverse-axis
    ///orthonormalization, quaternion conversion, and unconditional activation.
    ///The scalar candidate conversion preserves the recovered comparisons and
    ///refinements; exact VMX gather-register encoding remains separately audited.
    void capture_physics_error(const Transform& target, const Transform& deck_part);

    ///82C041C0 returns the target unchanged while inactive. Otherwise the
    ///slow/fast graph is evaluated once per invocation, without a dt factor.
    Transform apply(const Transform& target, bool fast, const BoardAnimationSettings& settings);
};

///82C04270: target is compared with actual deck BODY COM, then all seven
///board bodies receive this XYZ velocity, leaving their angular rates alone.
std::array<float, 4> target_velocity(std::array<float, 4> target, std::array<float, 4> deck_body_position, float dt);

}  // namespace s3::physics::board_animation
