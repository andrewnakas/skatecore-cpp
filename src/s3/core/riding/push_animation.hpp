// Port of skate-core/src/riding/push_animation.rs.
// TU3 push animation calculations, upstream of the contact-driven push force.
// `0x82BAD658` maps held time to strength; `0x82BACF50` updates the three
// Andale parameters. These do not select an animation or synthesize root motion.
#pragma once
#include "s3/core/point_graph.hpp"

namespace s3::riding::push_animation {

using point_graph::PointGraph;

struct PushBlendParameters {
    float hstr_vel_b = 0.0f;
    float lstr_vel_b = 0.0f;
    /// A negative current value is the native uninitialized sentinel.
    float vel_e = 0.0f;
};
S3_DERIVE_EQ(PushBlendParameters, hstr_vel_b, lstr_vel_b, vel_e)

struct PushAnimationCurves {
    /// anim_motion/pushing, collection +640 (x +656, y +688).
    PointGraph<8> button_time_max{};
    /// Collection +560 (x +576, y +608).
    PointGraph<8> button_time_to_dv{};
    /// Collection +720 (x +736, y +768).
    PointGraph<8> blend_speed_over_frames{};
    /// Collection +800 (x +816, y +848).
    PointGraph<8> blend_acc_over_frames{};

    float strength(float held_seconds, float speed) const;

    /// Returns the values written to shared properties +8/+12/+16 and then
    /// published as HSTR_VEL_B, LSTR_VEL_B, VEL_E, in that order.
    PushBlendParameters update_blend(PushBlendParameters current, PushBlendParameters target, float forward_speed,
                                     float delta_seconds) const;
};

}  // namespace s3::riding::push_animation
