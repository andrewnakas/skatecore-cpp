// Port of skate-game/src/physics/handplant/ik.rs
//! HandPlantManager82D633B0 and Skeleton82BD9728/97D0, shared four-limb IK.
#include "s3/game/physics/handplant/ik.hpp"

#include "s3/game/physics/footplant/math.hpp"
#include "s3/game/physics/handplant.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::handplant::ik {

using namespace ::s3::game::physics::footplant::math;

void update(SkaterRuntime& skater, bool ground) {
    const auto& p = skater.player_input.processed;
    auto& h = skater.handplant;
    if (ground && ((p.flags_2480 & (1u << 29)) != 0 || (p.flags_2480 & (1u << 28)) == 0)) {
        h.reset_ik();
    }
    const bool right = ((h.flags & 0x20000000u) != 0) ^ ((p.flags_2476 & (1u << 2)) != 0);
    const std::size_t side = right ? 1 : 0;
    const std::size_t limb = 2 + side;
    const auto& animated = skater.animated_skeleton;
    auto world = [&](std::size_t part) {
        return point(animated.roots.animation_to_world, animated.record.pose[part][3]);
    };
    V hand = world(3 + side * 4);
    const V shoulder = world(6 + side * 4);
    const bool attached = h.phase > -h.settings.hand_release;
    if (!attached) {
        h.ik_released = true;
    }
    const V shoulder_delta = sub(shoulder, h.anchor);
    if (!h.ik_released && length(shoulder_delta) > rs::from_bits(0x37800000u)) {
        const V axis = normalize(shoulder_delta);
        const float projection = dot(sub(hand, h.anchor), axis);
        if (projection <= 0.0f) {
            h.ik_released = true;
        } else {
            const V on_axis = madd(axis, projection, h.anchor);
            if (h.ik_latched) {
                hand = on_axis;
            } else {
                const V radial = sub(hand, on_axis);
                const float radius = h.settings.hand_radius.evaluate(projection);
                if (length(radial) > radius) {
                    hand = madd(normalize(radial), radius, on_axis);
                }
            }
        }
        const V delta = sub(hand, h.anchor);
        const float distance = length(delta);
        if (h.ik_distance < 0.0f) {
            h.ik_distance = distance;
        }
        h.ik_distance = rs::min(distance, h.ik_distance - h.settings.hand_approach);
        if (h.ik_distance < rs::from_bits(0x37800000u) || distance < rs::from_bits(0x37800000u)) {
            h.ik_released = true;
        } else {
            hand = madd(normalize(delta), h.ik_distance, h.anchor);
        }
        if (!h.ik_released && !h.ik_latched) {
            const V on_axis = madd(axis, dot(sub(hand, h.anchor), axis), h.anchor);
            if (length(sub(hand, on_axis)) < 0.02f) {
                h.ik_latched = true;
            }
        }
    }
    if (h.ik_released) {
        hand = h.anchor;
    }
    const V delta = sub(hand, shoulder);
    if (length(delta) > 0.65f) {
        hand = madd(normalize(delta), 0.65f, shoulder);
    }
    h.ik_blend = clamp01(h.ik_blend + (attached ? STEP / h.settings.hand_into : -STEP / h.settings.hand_out));
    if (h.ik_blend > 0.0f) {
        //82BD9728 clamps against the authored reparented hand target.
        const V original = point(animated.roots.animation_to_world, animated.targets[limb][3]);
        const V d = sub(hand, original);
        if (length(d) > 0.65f) {
            hand = madd(normalize(d), 0.65f, original);
        }
        skater.foot_ik.state.external_targets[limb].world_position = hand;
        skater.foot_ik.state.limbs[limb].external_target_set = true;
        skater.foot_ik.state.limbs[limb].target_blend = h.ik_blend;
    }
}

}  // namespace s3::game::physics::handplant::ik
