// Port of skate-game/src/physics/climbing/mod.rs
//! Hybrid ledge climbing. This extension owns movement while attached; the
//! stock action/motion graphs resume on release. It does not forge stock states
//! or animation-bank records, and uses the same skeleton and render snapshots.
#include <algorithm>
#include <cctype>
#include <utility>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/animation.hpp"
#include "s3/game/glam_ext.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/camera_output.hpp"
#include "s3/game/physics/climbing.hpp"
#include "s3/game/physics/colliders.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/player_state.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::climbing {

using glam::Mat3;
using glam::Quat;
using sc::math::Basis3;
using sc::math::Vector3;
using sc::physics::drive_frames::RetailAffineTransform;

namespace {
bool eq_ignore_ascii_case(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char x = static_cast<unsigned char>(a[i]);
        const unsigned char y = static_cast<unsigned char>(b[i]);
        const unsigned char lx = (x >= 'A' && x <= 'Z') ? static_cast<unsigned char>(x + 32) : x;
        const unsigned char ly = (y >= 'A' && y <= 'Z') ? static_cast<unsigned char>(y + 32) : y;
        if (lx != ly) return false;
    }
    return true;
}
}  // namespace

rs::Result<Runtime, std::string> Runtime::load(const sd::Vfs& vfs, const std::string& root,
                                               const std::vector<std::string>& names) {
    S3G_TRY(std::optional<Clips> clips, Clips::load(vfs, root));
    std::vector<std::size_t> indices;
    if (clips) {
        for (const auto& name : clips->reach.names) {
            std::optional<std::size_t> found;
            for (std::size_t i = 0; i < names.size(); ++i) {
                if (eq_ignore_ascii_case(names[i], name)) {
                    found = i;
                    break;
                }
            }
            if (!found) return rs::make_err(fmt::cat("Climbing rig bone ", name, " missing from skater"));
            indices.push_back(*found);
        }
    }
    Runtime r{};
    r.clips = std::move(clips);
    r.indices = std::move(indices);
    return r;
}

Mat4 matrix(const NativeMatrix& m) { return animation::native_matrix(m); }

NativeMatrix native(const Mat4& m) {
    NativeMatrix result = glam::to_cols_array_2d(m);
    // Native physical frames use vector padding, not a homogeneous position.
    // A one in this lane becomes a false 60 m/s root derivative on handoff.
    result[3][3] = 0.0f;
    return result;
}

std::array<float, 4> point(Vec3 p) { return {p.x, p.y, p.z, 0.0f}; }

float smooth(float t_) {
    const float t = rs::clamp(t_, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

bool pressed(const PlayerControls& controls, std::uint32_t bit) {
    const auto& w = controls.controller.words();
    return (w[13] & (1u << bit)) != 0 && (w[6] & (1u << bit)) == 0;
}

/// Called before native animation/input/solve. Attached motion has a single
/// owner; no gravity, foot IK, or stock root motion competes with hand contact.
rs::Result<bool, std::string> advance(GamePhysics& physics, SkaterRuntime& skater, const PlayerControls& controls,
                                      camera::CameraRuntime& camera) {
    Runtime runtime = std::exchange(skater.climbing, Runtime{});
    auto result = runtime.advance(physics, skater, controls, camera);
    skater.climbing = std::move(runtime);
    return result;
}

rs::Result<bool, std::string> Runtime::advance(GamePhysics& physics, SkaterRuntime& skater,
                                               const PlayerControls& controls, camera::CameraRuntime& camera) {
    const float dt = physics.settings.step.simulation.time_step;
    if (skater.player_input.pending_teleport().has_value()) {
        active.reset();
        approach.reset();
        cooldown = 0.3f;
        return false;
    }
    cooldown = rs::max(cooldown - dt, 0.0f);
    if (!clips) return false;
    const clip::Clip& c = clips->reach;
    if (!active) return false;
    Attached& a = *active;
    // A press during catch settling is not buffered into Mantle. Holding takeoff's
    // X therefore never skips the user's explicit hanging step.
    if (a.phase == Phase::Hang && pressed(controls, 23) && ledge::clear(physics.world, a.ledge)) {
        a.phase = Phase::Mantle;
        a.time = 0.0f;
        log::info("Climbing: pull up");
    }
    a.time += dt;
    const Quat yaw = glam::quat_from_rotation_y(rs::atan2(a.ledge.forward.x, a.ledge.forward.z));
    const auto hang = c.sample(c.duration());
    const auto hang_globals = c.globals(hang);
    const Vec3 hang_root = a.ledge.anchor - yaw * c.hands(hang_globals) + yaw * contacts::clearance(c, hang_globals);
    const clip::Clip& mantle = clips->mantle;
    const auto mantle_end = mantle.sample(mantle.duration());
    bool complete = false;
    const Phase sample_phase = a.phase;
    const float sample_time = a.time;
    std::vector<Transform> locals;
    Transform root;
    switch (a.phase) {
        case Phase::Catch: {
            // Give a high catch time to lower the body beneath its hands.
            // A fixed short blend made a tucked stock jump drop too fast.
            const float duration =
                rs::min(0.22f + glam::distance(a.start_root.translation, hang_root) * 0.18f, 0.5f);
            const float t = rs::min(a.time / duration, 1.0f);
            locals = hang;
            const float weight = smooth(t);
            for (std::size_t k = 0; k < locals.size() && k < a.entry.size(); ++k) {
                locals[k] = blend(a.entry[k], locals[k], weight);
            }
            root = Transform{};
            root.translation = glam::lerp(a.start_root.translation, hang_root, weight);
            root.rotation = glam::slerp(a.start_root.rotation, yaw, weight);
            if (t >= 1.0f) {
                a.phase = Phase::Hang;
                a.time = 0.0f;
                log::info("Climbing: hanging; press X to climb");
            }
            break;
        }
        case Phase::Hang: {
            locals = hang;
            root = Transform::from_translation(hang_root).with_rotation(yaw);
            break;
        }
        case Phase::Mantle: {
            locals = mantle.sample(a.time);
            for (std::size_t k = 0; k < locals.size() && k < hang.size(); ++k) {
                locals[k] = blend(hang[k], locals[k], smooth(a.time / 0.22f));
            }
            const auto g = c.globals(locals);
            // Lock the hands through the weight-bearing half, then release
            // into the authored vault and warp feet onto the clear top.
            const Vec3 attached = a.ledge.anchor - yaw * c.hands(g) + yaw * contacts::clearance(c, g);
            const Vec3 end_attached =
                a.ledge.anchor - yaw * c.hands(c.globals(mantle.sample(mantle.duration() * 0.55f)));
            const float release = smooth((a.time / mantle.duration() - 0.55f) / 0.45f);
            const Vec3 final_root = a.ledge.landing - yaw * c.feet(c.globals(mantle_end));
            const Vec3 translation = release > 0.0f ? glam::lerp(end_attached, final_root, release) : attached;
            if (a.time >= mantle.duration()) {
                a.phase = Phase::Settle;
                a.time = 0.0f;
            }
            root = Transform::from_translation(translation).with_rotation(yaw);
            break;
        }
        case Phase::Settle: {
            const float weight = smooth(a.time / 0.25f);
            for (std::size_t k = 0; k < mantle_end.size() && k < ground_entry.size(); ++k) {
                locals.push_back(blend(mantle_end[k], ground_entry[k], weight));
            }
            const Vec3 translation = a.ledge.landing - yaw * c.feet(c.globals(locals));
            complete = a.time >= 0.25f;
            root = Transform::from_translation(translation).with_rotation(yaw);
            break;
        }
    }
    // Store and publish the actual bones into the existing physical body.
    // Board helpers are never driven by the Mixamo board-at-origin track.
    const Mat4 root_matrix = root.to_matrix();
    float contact_weight = 0.0f;
    switch (sample_phase) {
        case Phase::Catch:
            contact_weight = 1.0f;
            break;
        case Phase::Hang:
            contact_weight = 1.0f;
            break;
        case Phase::Mantle:
            contact_weight = 1.0f - smooth((sample_time / mantle.duration() - 0.50f) / 0.05f);
            break;
        case Phase::Settle:
            contact_weight = 0.0f;
            break;
    }
    if (contact_weight > 0.0f) contacts::hands(c, locals, root_matrix, a.ledge, contact_weight);
    auto globals = c.globals(locals);
    const std::size_t board_index = c.index("SKATEBOARD_ROOT");
    Mat4 board_world{};
    if (a.carry_board) {
        const Vec3 hips = glam::transform_point3(root_matrix, glam::truncate(globals[c.index("HIPS")].w_axis));
        const Vec3 chest = glam::transform_point3(root_matrix, glam::truncate(globals[c.index("SPINE3")].w_axis));
        const Vec3 up = glam::normalize(chest - hips);
        const Vec3 shoulders = glam::transform_vector3(
            root_matrix,
            glam::truncate(globals[c.index("LEFTARM")].w_axis) - glam::truncate(globals[c.index("RIGHTARM")].w_axis));
        const Vec3 forward = glam::normalize(glam::cross(shoulders, up));
        const Vec3 right = glam::normalize(glam::cross(up, forward));
        const Transform stowed = Transform::from_translation((hips + chest) * 0.5f - forward * 0.20f)
                                     .with_rotation(glam::quat_from_mat3(Mat3::from_cols(right, -forward, up)));
        Transform board = blend(Transform::from_matrix(a.board_world), stowed,
                                rs::max(smooth(a.time / 0.2f), a.phase != Phase::Catch ? 1.0f : 0.0f));
        if (sample_phase == Phase::Settle) {
            const Mat4 hand_carry = root_matrix * c.globals(ground_entry)[board_index];
            board = blend(board, Transform::from_matrix(hand_carry), smooth(sample_time / 0.25f));
        }
        board_world = board.to_matrix();
    } else {
        // A dropped board remains a physical object while the skater is
        // attached. Continue its gravity/collisions independently.
        physics.board.clear_forces();
        S3G_CHECK(physics.riding.start_wheel_queries(physics.board, physics.world));
        S3G_CHECK(physics.riding.finish_wheel_queries());
        const auto volumes = colliders::world_volumes(physics.board, physics.settings);
        const auto collisions_slice = physics.world.query_primitives(
            sc::physics::Slice<const sc::physics::board_world::BoardWorldVolume>(volumes), physics.query,
            physics.retention);
        const std::vector<sc::physics::board_world::BoardCollision> collisions(collisions_slice.begin(),
                                                                                collisions_slice.end());
        physics.contact_count = collisions.size();
        physics.board.advance(sc::physics::Slice<const sc::physics::board_world::BoardCollision>(collisions),
                              {0.0f, 0.0f}, physics.settings.step);
        S3G_CHECK(physics.riding.finish_post_physics(physics.board, physics.board_wiping_out,
                                                     physics.processed_flags_2468, dt));
        board_world = matrix(solve::deck_frame(physics.board)) * a.physical_board_world.inverse() * a.board_world;
    }
    const Mat4 board_delta = root_matrix.inverse() * board_world * globals[board_index].inverse();
    for (std::size_t i = 0; i < globals.size(); ++i) {
        std::int32_t parent = std::int32_t(i);
        while (parent >= 0 && parent != std::int32_t(board_index)) parent = c.parents[std::size_t(parent)];
        if (parent == std::int32_t(board_index)) globals[i] = board_delta * globals[i];
    }
    std::vector<NativeMatrix> pose = a.fallback;
    for (std::size_t k = 0; k < indices.size() && k < globals.size(); ++k) pose[indices[k]] = native(globals[k]);
    const Mat4 actual_board = board_world * a.board_world.inverse() * a.physical_board_world;
    if (a.carry_board) {
        physics.board.set_transform(RetailAffineTransform{
            Basis3{{glam::truncate(actual_board.x_axis).to_array(), glam::truncate(actual_board.y_axis).to_array(),
                    glam::truncate(actual_board.z_axis).to_array()}},
            Vector3::new_(actual_board.w_axis.x, actual_board.w_axis.y, actual_board.w_axis.z)});
        for (auto& b : physics.board.bodies_mut()) zero_rates(b.rates);
    }
    S3G_CHECK(publish_pose(physics, skater, root_matrix, std::move(pose)));
    // Camera still uses its normal graph and collision avoidance. Read live
    // right stick while the stock animation graph is temporarily paused.
    const auto& w = controls.controller.words();
    skater.animation_input.extra.look_x = rs::from_bits(w[9]);
    skater.animation_input.extra.look_y = rs::from_bits(w[10]);
    const auto feedback = skater.physical_feedback;
    S3G_CHECK(camera_output::advance(physics, skater, feedback, camera));
    physics.ticks += 1;
    physics.clock.finish_tick();
    if (complete) {
        // Re-seed the real ground controller at the landing. Old ground
        // query/velocity history must not pull the skater back to takeoff.
        S3G_CHECK(player_state::resume_after_climb(physics, skater));
        active.reset();
        cooldown = 0.3f;
        log::info("Climbing: returned to walking");
    }
    return true;
}

void zero_rates(::s3::physics::rigid_body::RetailBodyRates& r) {
    r.linear_velocity = Vector3::ZERO;
    r.angular_velocity = Vector3::ZERO;
    r.force_acceleration = Vector3::ZERO;
    r.torque_acceleration = Vector3::ZERO;
}

Status publish_pose(const GamePhysics& physics, SkaterRuntime& skater, Mat4 root, std::vector<NativeMatrix> pose) {
    auto& s = skater.animated_skeleton;
    auto parts_r = sc::physics::skeleton_animation_record::map_animation_parts(pose, s.bone_indices, s.physics_frames);
    if (parts_r.is_err()) return rs::make_err(std::string(parts_r.unwrap_err()));
    const auto parts = parts_r.unwrap();
    s.roots.animation_to_world = native(root);
    s.roots.world_to_animation = native(root.inverse());
    s.record.update(parts, s.roots.animation_to_board, s.masses);
    s.animation_hips = parts[23];
    s.animation_board = parts[0];
    const auto& com_src = s.record.centre_of_mass;
    const Vec3 com = glam::transform_point3(root, Vec3::new_(com_src[0], com_src[1], com_src[2]));
    s.board_frames.update_com_lift(native(root), point(com), 0.24f);
    s.board_frames.centre_of_mass = point(com);
    s.board_frames.previous_centre_of_mass = point(com);
    s.board_frames.com_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    for (std::size_t i = 0; i < parts.size(); ++i) {
        skater.skeleton.set_part_transform(i, native(root * matrix(parts[i])));
    }
    // The two auxiliary COM bodies participate in the same constraints as the
    // visible bones. Leaving them at takeoff causes a false collision impulse.
    skater.skeleton.set_part_transform(24, s.board_frames.lifted_com_frame);
    skater.skeleton.set_part_transform(25, s.board_frames.com_frame);
    skater.skeleton_drives.targets.reset(native(root * matrix(parts[23])), native(root * matrix(parts[0])));
    const auto targets = skater.skeleton_drives.targets.update_extra_targets(skater.skeleton, s.board_frames.com_frame,
                                                                             s.board_frames.lifted_com_frame);
    skater.skeleton_input.extra_target_positions = {targets.com, targets.lifted_com, targets.following_com};
    skater.pose_errors.set_targets(targets);
    skater.skeleton_input.drive_frames = parts;
    for (auto& b : skater.skeleton.bodies_mut()) zero_rates(b.rates);
    skater.skeleton.animation_to_world = native(root);
    skater.skeleton_input.previous_root_position = point(glam::truncate(root.w_axis));
    skater.skeleton_input.root_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    skater.skeleton_output.correction = {};
    skater.skeleton.publish_physical_record(native(root * matrix(parts[0])));
    auto& p = skater.player_input.processed;
    {
        const NativeMatrix n = native(root);
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = 0; j < 4; ++j) p.effective_anim_transform_192[i][j] = rs::to_bits(n[i][j]);
    }
    {
        const auto cp = point(com);
        for (std::size_t j = 0; j < 4; ++j) p.vectors_544_560_592_608[2][j] = rs::to_bits(cp[j]);
    }
    p.vectors_544_560_592_608[3] = {0u, 0u, 0u, 0u};
    p.flags_2472 &= ~(1u << 10);
    auto& physical = skater.player_input.physical;
    {
        const NativeMatrix n = native(root);
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = 0; j < 4; ++j) physical.skeleton.anim_to_world_11920[i][j] = rs::to_bits(n[i][j]);
    }
    {
        const auto cp = point(com);
        for (std::size_t j = 0; j < 4; ++j) physical.reckoning.vector_64[j] = rs::to_bits(cp[j]);
    }
    physical.reckoning.vector_16 = {0u, 0u, 0u, 0u};
    physical.reckoning.vector_96 = {rs::to_bits(0.0f), rs::to_bits(1.0f), rs::to_bits(0.0f), rs::to_bits(0.0f)};
    physical.air.flag_441 = 0;
    physical.air.use_air_reckoning_452 = 0;
    skater.centre_of_mass_output = skater.centre_of_mass_filter.update(point(com), {0.0f, 0.0f, 0.0f, 0.0f});
    skater.render_pose = std::move(pose);
    skater.pose_generation = skater.pose_generation + 1u;  // wrapping_add on u64
    skater.player_input.player.update_count_1316 = skater.player_input.player.update_count_1316 + 1u;
    (void)physics;
    return ok();
}

}  // namespace s3::game::physics::climbing
