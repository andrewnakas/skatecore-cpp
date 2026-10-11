// Port of simref sim/src/camera/publication.rs.
//! Game owners -> normal camera subject82DF69C0. Call after physical output.
//! Missing publications are explicit inputs, never inferred from render pose.
//
// The publication input records are plain data. `snapshot` reads the stage-P3 skate-game
// owners (GamePhysics / SkaterRuntime, not ported yet), so it is a template over those
// types: it is only compiled when a host instantiates it, and every access below follows
// the Rust field paths verbatim (`physics.riding.motion.effective_basis`, ...).
#pragma once
#include <array>
#include <cstdint>
#include <sstream>
#include <string>

#include "s3/core/camera/anchors.hpp"
#include "s3/core/camera/compass.hpp"
#include "s3/core/camera/manager_subject.hpp"
#include "s3/core/camera/subject.hpp"
#include "s3/core/camera/subject_pose.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/game/camera/graph_subject.hpp"
#include "s3/game/camera/subject.hpp"

namespace s3::game::camera {

using s3::camera::F4;
using s3::camera::F4x4;

/// State record fields absent from the input phase's smaller CurrentStateFields.
/// The same output/reset owner must publish these; there is no camera default.
struct CameraStateOutput {
    float height_32 = 0.0f;
    std::uint8_t physically_pushing_55 = 0;
    std::uint8_t wiping_out_59 = 0;
    std::uint8_t manual_60 = 0;
    std::uint8_t reset_62 = 0;
    std::uint8_t use_skeleton_root_75 = 0;
    std::uint8_t flag_79 = 0;
    std::uint8_t flag_81 = 0;
};

struct CameraAnimationOutput {
    /// PhysOutAnimation32..60 from the existing turn conditioner, once per tick.
    std::array<float, 8> conditioned_turn{};
    float input_turn_64 = 0.0f;
    float input_kickturn_68 = 0.0f;
    float time_since_input_128 = 0.0f;
    std::uint32_t wipeout_tweak_148 = 0;
    std::uint8_t stance_155 = 0;
    std::uint8_t running_out_160 = 0;
    /// Actor's actual SkaterAnim component virtual28, copied at82DF6EA4.
    std::uint8_t skater_animation_stance = 0;
};

/// Air output2. The ordinary Ground cancellation/reset owns its retained
/// values and validity; a false validity flag does not erase these vectors.
struct CameraAirOutput {
    F4 apex_0{};
    F4 landing_position_16{};
    F4 landing_normal_32{};
    F4 launch_position_48{};
    F4 heading_80{};
    float time_176 = 0.0f;
    float duration_180 = 0.0f;
    float apex_time_196 = 0.0f;
    std::uint8_t flag_440 = 0;
};

/// The alternate trajectory and event fields read from OffBoard output18.
struct CameraOffboardOutput {
    float duration_92 = 0.0f;
    float time_152 = 0.0f;
    float apex_time_156 = 0.0f;
    F4 launch_normal_160{};
    F4 launch_position_176{};
    F4 landing_normal_192{};
    F4 landing_position_208{};
    F4 heading_224{};
    F4 apex_240{};
    std::uint8_t object_held_304 = 0;
    std::uint8_t hurdle_317 = 0;
    std::uint8_t use_trajectory_331 = 0;
    std::uint8_t dropping_in_334 = 0;
};

/// Grinds output4, distinct from Ground output8 (whose316 is wall-ride exit).
struct CameraGrindOutput {
    F4 direction_0{};
    F4 camera_target_96{};
    std::uint8_t grinding_316 = 0;
};

struct CameraEventsOutput {
    std::uint8_t intent_51 = 0;
    std::uint8_t preparing_52 = 0;
    std::uint8_t dropping_in_63 = 0;
    std::uint8_t trick_125 = 0;
    std::uint8_t hippy_jump_322 = 0;
    float broken_bone_duration_200 = 0.0f;
    std::uint32_t capabilities_204 = 0;
};

/// Engine-owned preference values copied by the original subject publisher.
struct CameraPreferences {
    std::array<bool, 2> invert_look{};
    std::uint8_t shake_variant = 0;
    float value_32 = 0.0f;
};

/// Remaining real output producers needed alongside the existing game owners.
/// This structure deliberately has no Default in Rust: its callers must publish the
/// actual native reset/state values even when air/offboard/grinds are inactive.
struct CameraPublicationInputs {
    /// Completed physics tick represented by these publication inputs.
    std::uint64_t tick = 0;
    CameraStateOutput state;
    CameraAnimationOutput animation;
    CameraAirOutput air;
    CameraOffboardOutput offboard;
    CameraGrindOutput grinds;
    CameraEventsOutput events;
    /// PhysOutSystemReckoning80, after its own physical output conditioner.
    F4 damped_com_80{};
    /// PhysOutGround80 and288.80 differs from dynamic up64 and wheel normal96.
    F4 ground_up_80{};
    float ground_scalar_288 = 0.0f;
    /// PhysOutSkeleton552/556 and PhysOutCollision64, respectively.
    std::array<float, 2> look_552_556{};
    F4 collision_look_target_64{};
    CameraPreferences preferences;
    /// Our stable player/query identity replaces the native actor address.
    std::uint32_t context = 0;
};

namespace publication {

inline F4x4 effective_skeleton_root(F4x4 root, std::uint32_t flags_2476) {
    if ((flags_2476 & 4) != 0) {
        root[0] = rs::map(root[0], [](float v) { return -v; });
        root[2] = rs::map(root[2], [](float v) { return -v; });
    }
    return root;
}

inline std::string debug_lanes(const F4& v) {
    std::ostringstream os;
    os << "[" << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << "]";
    return os.str();
}

/// Publish from the very same physical bodies and completed output records
/// used by the animation phase. This neither advances physics nor conditions
/// the steering twice. SubjectPublisher owns the subsequent camera histories.
/// (Rust `snapshot`, re-exported as `publish_camera_subject`.)
template <class GamePhysics, class SkaterRuntime>
rs::Result<CameraSubjectSnapshot, std::string> snapshot(const GamePhysics& physics, const SkaterRuntime& skater,
                                                        const CameraPublicationInputs& input) {
    namespace cam = s3::camera;
    const auto& p = skater.player_input.physical;
    const auto& processed = skater.player_input.processed;
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Camera publication requires the completed player input toolkit"));
    }
    const auto& toolkit = *skater.player_input.toolkit;
    const auto ground = skater.ground.output(processed, skater.animation_input, toolkit);
    const auto deck =
        physics.board.part_transforms()[std::size_t(s3::physics::board::BodyId::Deck)];
    F4x4 physical_transform{};
    for (std::size_t column = 0; column < 3; ++column) {
        const auto& axis = physics.riding.motion.effective_basis.columns[column];
        for (std::size_t i = 0; i < 3; ++i) physical_transform[column][i] = axis[i];
    }
    physical_transform[3] = {deck.translation.x, deck.translation.y, deck.translation.z, 0.0f};
    //82BE3650 -> PhysOutSkeleton0/432 at82BE20F4/2100. Its stance flag is
    //Processed2476 bit2, distinct from the board's effective-frame flag2468.
    const F4x4 skeleton_root =
        effective_skeleton_root(skater.animated_skeleton.roots.animation_to_world, processed.flags_2476);
    //82DF80D8 selects the subject pose separately.82DF70CC/70E8 still
    //publish the board transform/position, including while off-board.
    const auto& record = skater.skeleton.record;
    const F4 com = rs::from_bits(p.reckoning.vector_64);
    const F4 up = rs::from_bits(p.reckoning.vector_96);
    const F4 velocity = rs::from_bits(p.skateboard.vector_80);
    //The source getter396 is bound to Motion64, despite its camera name
    //"acceleration". Motion64 is the published deck angular velocity.
    const F4 acceleration = rs::from_bits(p.skateboard.vector_64);
    // Native vector storage retains four lanes, but camera positions,
    // directions and magnitudes use only XYZ. W is not a homogeneous
    // coordinate here and can be non-finite without corrupting the pose.
    auto finite = [&](const char* name, const F4& values) -> rs::Result<rs::Unit, std::string> {
        if (rs::is_finite(values[0]) && rs::is_finite(values[1]) && rs::is_finite(values[2])) {
            return rs::Unit{};
        }
        return rs::make_err(std::string("Camera subject owner published non-finite ") + name + ": " +
                            debug_lanes(values));
    };
    S3_CAM_TRY(rs::Unit c0, finite("centre_of_mass", com));
    S3_CAM_TRY(rs::Unit c1, finite("reckoning_up", up));
    S3_CAM_TRY(rs::Unit c2, finite("damped_centre_of_mass", input.damped_com_80));
    S3_CAM_TRY(rs::Unit c3, finite("ground_up", input.ground_up_80));
    (void)c0, (void)c1, (void)c2, (void)c3;
    for (const auto& column : skeleton_root) {
        S3_CAM_TRY(rs::Unit c, finite("skeleton_root", column));
        (void)c;
    }
    const std::pair<std::size_t, const char*> bones[] = {
        {1, "head"}, {15, "left_foot"}, {19, "right_foot"}, {23, "hips"}};
    for (const auto& [index, name] : bones) {
        S3_CAM_TRY(rs::Unit c, finite(name, record.pose[index][3]));
        (void)c;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        if (!rs::is_finite(velocity[i]) || !rs::is_finite(acceleration[i])) {
            return rs::make_err("Camera received non-finite board motion publication: velocity=" +
                                debug_lanes(velocity) + "; acceleration=" + debug_lanes(acceleration));
        }
    }
    const F4 last_ground_up = rs::from_bits(p.ground.vector_96);
    const auto category = p.state.category_12;
    const auto state = p.state.state_16;
    const CameraOffboardOutput& off = input.offboard;
    const CameraAirOutput& air = input.air;
    const bool alternate = off.use_trajectory_331 != 0;
    const F4 launch_position = alternate ? off.launch_position_176 : air.launch_position_48;
    const F4 launch_normal = alternate ? off.launch_normal_160 : last_ground_up;
    const F4 landing_position = alternate ? off.landing_position_208 : air.landing_position_16;
    const F4 landing_normal = alternate ? off.landing_normal_192 : air.landing_normal_32;
    const F4 apex_position = alternate ? off.apex_240 : air.apex_0;
    const F4 heading = alternate ? off.heading_224 : air.heading_80;
    const float trajectory_time = alternate ? off.time_152 : air.time_176;
    const float trajectory_duration = alternate ? off.duration_92 : air.duration_180;
    const float apex_time = alternate ? off.apex_time_156 : air.apex_time_196;
    const std::uint8_t grinding = input.grinds.grinding_316;
    const std::array<float, 2> look = {
        input.preferences.invert_look[0] ? -input.look_552_556[0] : input.look_552_556[0],
        input.preferences.invert_look[1] ? -input.look_552_556[1] : input.look_552_556[1],
    };
    cam::ManagerSubject subject;
    //Overwritten by the stock subject's one-frame publication history.
    subject.rig.transform = physical_transform;
    subject.rig.skeleton_root = skeleton_root;
    subject.rig.hips_position = record.pose[23][3];
    subject.rig.last_valid_ground_up = last_ground_up;
    subject.rig.reference_positions = {};
    subject.rig.context = input.context;
    subject.rig.pumping_acceleration = skater.ground.pumping.pump_acceleration;
    subject.rig.state_height_32 = input.state.height_32;
    subject.rig.in_ground_physics = category == 100 ? 1 : 0;
    subject.rig.grinding = grinding;
    // KnownAir Fill publishes validity at437. Byte441 is the body-flip
    // flag; using it hides ordinary ollie trajectories from the camera.
    subject.rig.trajectory_valid = alternate ? std::uint8_t(1) : std::uint8_t(p.air.known_air_valid_437);
    subject.rig.wiping_out = input.state.wiping_out_59;
    subject.rig.physically_pushing = input.state.physically_pushing_55;
    subject.rig.at_pushable_speed = ground.skateboard_motion_4.is_at_pushable_speed ? 1 : 0;
    subject.rig.off_board = category == 500 ? 1 : 0;
    subject.rig.air_flag_452 = p.air.use_air_reckoning_452;
    subject.rig.state_flag_81 = input.state.flag_81;
    subject.rig.broken_bone_slowmo =
        (input.events.broken_bone_duration_200 > 0.0f && (input.events.capabilities_204 & 4) != 0) ? 1 : 0;
    subject.rig.subject_flag_328 = 0;  //82DF726C clears this every publication.
    subject.anchors = cam::Anchors::new_().entries;  //filled by the real anchor histories.
    subject.compass = {};                             //filled by the real Compass owner.
    subject.board_offset_direction = up;
    subject.ground_normal = input.ground_up_80;
    subject.launch_position = launch_position;
    subject.launch_normal = launch_normal;
    subject.landing_position = landing_position;
    subject.landing_normal = landing_normal;
    subject.apex_position = apex_position;
    subject.direction_424 = input.grinds.direction_0;
    subject.look = look;
    subject.steering = {input.animation.conditioned_turn[4], input.animation.conditioned_turn[3],
                        input.animation.input_turn_64, input.animation.input_kickturn_68};
    subject.trajectory_time = trajectory_time;
    subject.trajectory_duration = trajectory_duration;
    subject.apex_time = apex_time;
    subject.value_512 = input.preferences.value_32;
    subject.value_516 = input.ground_scalar_288;
    subject.reset = input.state.reset_62;
    subject.flag_556 = air.flag_440;
    subject.stance_560 = input.animation.stance_155;
    subject.stance_592 = input.animation.skater_animation_stance;
    subject.flag_652 = input.state.flag_79;
    subject.shake_variant = input.preferences.shake_variant;
    subject.special_effect = 0;  //82DF6A44..6A60 clears observer mode for this subject.
    subject.flag_684 = off.object_held_304;

    CameraSubjectSnapshot out;
    out.tick = input.tick;
    out.subject = subject;
    out.pose.physical_transform = physical_transform;
    out.pose.skeleton_root = skeleton_root;
    out.pose.center_of_mass = com;
    out.pose.reckoned_center_of_mass = input.damped_com_80;
    out.pose.wiping_out = input.state.wiping_out_59 != 0;
    out.pose.state_flag_75 = input.state.use_skeleton_root_75 != 0;
    out.anchors.board_position = physical_transform[3];
    out.anchors.board_velocity = velocity;
    out.anchors.board_acceleration = acceleration;
    out.anchors.board_offset_direction = up;
    out.anchors.center_of_mass = com;
    out.anchors.damped_center_of_mass = input.damped_com_80;
    out.anchors.skeleton_root_up = skeleton_root[1];
    out.anchors.grind_point = input.grinds.camera_target_96;
    out.anchors.grinding = grinding != 0;
    out.anchors.reset = input.state.reset_62 != 0;
    //82BE1E64..1ED4 copies these actual physical volume positions.
    out.reference_points.head = record.pose[1][3];
    out.reference_points.hips = record.pose[23][3];
    out.reference_points.left_foot = record.pose[15][3];
    out.reference_points.right_foot = record.pose[19][3];
    out.reference_points.board = skeleton_root[3];
    out.reference_points.centre_of_mass = com;
    out.reference_points.damped_centre_of_mass = input.damped_com_80;
    out.reference_points.grind_position = input.grinds.camera_target_96;
    out.reference_points.tracked_anchor = {};
    out.reference_points.incline_normal = {};
    out.reference_points.grinding = grinding;
    out.compass.skeleton_direction = skeleton_root[2];
    out.compass.look_target = input.collision_look_target_64;
    out.compass.board_velocity = velocity;
    out.compass.trajectory_direction = heading;
    out.compass.state_103 = state == 103;
    out.graph.time_since_player_input = input.animation.time_since_input_128;
    out.graph.wipeout_tweak = input.animation.wipeout_tweak_148;
    out.graph.onboard_air = category == 200;
    out.graph.preparing_to_jump = input.events.preparing_52 != 0;
    out.graph.manual = input.state.manual_60 != 0 && input.events.intent_51 == 0;
    out.graph.offboard_air = state == 501;
    out.graph.running_out = input.animation.running_out_160 != 0;
    out.graph.footplant = state == 601;
    out.graph.handplant = state == 600;
    out.graph.hippy_jump = input.events.hippy_jump_322 != 0;
    out.graph.hippy_hurdle = off.hurdle_317 != 0;
    out.graph.boneless = input.events.trick_125 != 0;
    out.graph.moving_object = state == 502;
    out.graph.skitching = state == 104;
    out.graph.dropping_in = input.events.dropping_in_63 != 0 && off.dropping_in_334 != 0;
    out.graph.slow_motion_air_duration = p.air.scalar_184;
    return out;
}

}  // namespace publication
}  // namespace s3::game::camera
