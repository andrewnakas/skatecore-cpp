// Port of skate-game/src/physics/climbing/approach.rs
//! Airborne reach is a presentation overlay. Stock graphs, gravity and collisions
//! retain ownership until both arm targets are physically within reach.
#include "s3/game/physics/climbing/approach.hpp"

#include <string>
#include <utility>

#include "s3/game/glam_ext.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/climbing.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::climbing::approach {

using glam::Mat4;
using glam::Vec2;
using glam::Vec3;
using sc::player::state::PhysicalStateId;
using transform_shim::Transform;

void advance(const GamePhysics& physics, SkaterRuntime& skater, const PlayerControls& controls) {
    Runtime runtime = std::exchange(skater.climbing, Runtime{});
    update(runtime, physics, skater, controls);
    skater.climbing = std::move(runtime);
}

void update(Runtime& r, const GamePhysics& physics, SkaterRuntime& skater, const PlayerControls& controls) {
    if (!r.clips) return;
    const clip::Clip& c = r.clips->reach;
    const Mat4 root = matrix(skater.animated_skeleton.roots.animation_to_world);
    std::vector<Mat4> globals;
    globals.reserve(r.indices.size());
    for (std::size_t i : r.indices) globals.push_back(matrix(skater.render_pose[i]));
    std::vector<Transform> locals;
    locals.reserve(globals.size());
    for (std::size_t i = 0; i < globals.size(); ++i) {
        const Mat4 g = globals[i];
        locals.push_back(Transform::from_matrix(c.parents[i] < 0 ? g : globals[std::size_t(c.parents[i])].inverse() * g));
    }
    const PhysicalStateId state = skater.player_state.current();
    const auto reckoning_16 = skater.player_input.physical.reckoning.vector_16;
    const float ground_speed =
        glam::length(Vec3::new_(rs::from_bits(reckoning_16[0]), rs::from_bits(reckoning_16[1]),
                                rs::from_bits(reckoning_16[2])));
    if (state == PhysicalStateId::BipedGround && (r.ground_entry.empty() || ground_speed < 0.3f)) {
        r.ground_entry = locals;
        // Native walking roots sit on the support plane. Normalize the saved
        // return pose so a lifted foot in the run cycle cannot lower that root.
        const float height = c.feet(globals).y;
        for (std::size_t i = 0; i < r.ground_entry.size(); ++i) {
            if (c.parents[i] < 0) r.ground_entry[i].translation.y -= height;
        }
    }
    const bool airborne = state == PhysicalStateId::BipedAir;
    if ((!airborne && state != PhysicalStateId::BipedGround) || r.cooldown > 0.0f) {
        r.approach.reset();
        return;
    }
    const Vec3 feet = glam::transform_point3(root, c.feet(globals));
    const Vec3 velocity = Vec3::new_(rs::from_bits(reckoning_16[0]), rs::from_bits(reckoning_16[1]),
                                     rs::from_bits(reckoning_16[2]));
    std::optional<Vec3> direction_opt;
    if (controls.offboard_direction) {
        const auto& v = *controls.offboard_direction;
        const Vec3 d = Vec3::new_(v[0], 0.0f, v[2]);
        if (glam::length_squared(d) > 0.04f) direction_opt = d;
    }
    const Vec3 direction = direction_opt ? *direction_opt
                                         : glam::truncate(root.z_axis) *
                                               ((skater.player_input.processed.flags_2476 & 4u) != 0 ? -1.0f : 1.0f);
    const bool moving = airborne || ground_speed > 0.15f ||
                        (controls.offboard_direction &&
                         Vec2::new_((*controls.offboard_direction)[0], (*controls.offboard_direction)[2])
                                 .length_squared() > 0.04f);
    const std::optional<ledge::Ledge> found =
        moving ? ledge::find_air(physics.world, feet, direction) : std::optional<ledge::Ledge>();
    // Follow the approach while running. Freeze the two contacts on takeoff,
    // once the player has committed to this edge.
    if (!airborne) {
        if (r.approach && found) {
            if (glam::distance(r.approach->ledge.anchor, found->anchor) < 0.5f) r.approach->ledge = *found;
        }
    }
    if (!r.approach) {
        if (found) {
            if (velocity.dot(found->forward) >= -0.1f) r.approach = Approach{*found, 0.0f};
        }
    }
    if (!r.approach) return;
    Approach& a = *r.approach;
    const Vec3 separation = a.ledge.anchor - feet;
    const float distance = Vec2::new_(separation.x, separation.z).length();
    const bool valid = moving && separation.y > 0.35f && separation.y < 2.75f && distance < 2.7f &&
                       separation.dot(a.ledge.forward) > 0.0f &&
                       glam::normalize_or_zero(direction).dot(a.ledge.forward) > 0.65f &&
                       velocity.dot(a.ledge.forward) >= -0.3f && ledge::clear(physics.world, a.ledge);
    const float dt = physics.settings.step.simulation.time_step;
    // Continuous distance gain, starting during the run. Ground anticipation
    // is gentle; the jump smoothly builds from it into full hand contact.
    const float desired = valid ? reach_gain(distance, airborne) : 0.0f;
    a.weight += (desired - a.weight) * (1.0f - rs::exp(-dt / 0.10f));
    if (a.weight < 0.001f && !valid) {
        r.approach.reset();
        return;
    }
    const float weight = a.weight;
    // Only shoulders and arms borrow the authored reach. The stock jump keeps
    // its hips, legs, trajectory and timing, even during a failed attempt.
    const auto authored = c.sample(c.duration());
    for (const char* side : {"LEFT", "RIGHT"}) {
        for (const char* suffix : {"SHOULDER", "ARM", "FOREARM", "HAND"}) {
            const std::size_t i = c.index(std::string(side) + suffix);
            locals[i].rotation = glam::slerp(locals[i].rotation, authored[i].rotation, weight * 0.75f);
        }
    }
    const auto before_ik = c.globals(locals);
    bool reachable = true;
    {
        const std::array<const char*, 2> sides{"LEFT", "RIGHT"};
        for (std::size_t i = 0; i < 2; ++i) {
            const std::string side = sides[i];
            const Vec3 shoulder = glam::transform_point3(root, glam::truncate(before_ik[c.index(side + "ARM")].w_axis));
            const Vec3 elbow =
                glam::transform_point3(root, glam::truncate(before_ik[c.index(side + "FOREARM")].w_axis));
            const Vec3 hand = glam::transform_point3(root, glam::truncate(before_ik[c.index(side + "HAND")].w_axis));
            if (!(glam::distance(shoulder, contacts::wrist(a.ledge, i).first) <=
                  glam::distance(shoulder, elbow) + glam::distance(elbow, hand) - 0.015f)) {
                reachable = false;
                break;
            }
        }
    }
    contacts::hands(c, locals, root, a.ledge, weight);
    const auto output = c.globals(locals);
    for (std::size_t k = 0; k < r.indices.size() && k < output.size(); ++k) {
        skater.render_pose[r.indices[k]] = native(output[k]);
    }
    if (airborne && valid && reachable && a.weight >= 0.95f && !r.ground_entry.empty()) {
        const Mat4 deck = matrix(solve::deck_frame(physics.board));
        Attached attached;
        attached.phase = Phase::Catch;
        attached.time = 0.0f;
        attached.ledge = a.ledge;
        attached.start_root = Transform::from_matrix(root);
        attached.entry = locals;
        attached.fallback = skater.render_pose;
        attached.board_world = root * output[c.index("SKATEBOARD_ROOT")];
        attached.physical_board_world = deck;
        attached.carry_board = skater.skateboard_controller.fields.state_448 == 1;
        r.active = std::move(attached);
        r.approach.reset();
        skater.offboard_air_selector.reset();
        skater.offboard_feet.reset();
        skater.foot_ik.state.enable_feet(false);
        log::info("Climbing: caught ledge from stock jump");
    }
}

float reach_gain(float distance, bool airborne) {
    const float proximity = smooth((2.6f - distance) / 1.8f);
    return proximity * (airborne ? 1.0f : 0.45f);
}

}  // namespace s3::game::physics::climbing::approach
