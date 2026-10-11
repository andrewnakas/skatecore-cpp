// Port of skate-game/src/physics/riding_outputs.rs.
//! Live board/query/solver outputs used by the riding gameplay owner.
//! Physical calculations stay in core; this owns stock settings and scheduling
//! state. Solved contacts and line probes remain distinct native inputs.
#include "s3/game/physics/riding_outputs.hpp"

#include "s3/core/physics/board.hpp"

namespace s3::game::physics::riding_outputs {

using sc::math::Vector3;
using sc::physics::board::BodyId;
using sc::physics::board_ground::BoardGroundState;
using sc::physics::board_ground::WheelLineHit;
using sc::physics::board_ground::WheelLineState;
using sc::physics::board_motion_output::BoardMotionOutput;
using sc::point_graph::PointGraph;
using sc::riding::ground_orientation::GroundOrientation;
using sc::riding::ground_orientation::GroundOrientationInput;
using sc::riding::ground_orientation::GroundOrientationSettings;
using sc::riding::reckoning_frames::ReckoningFrames;
using sc::riding::speed_and_slope::SpeedAndSlopeSettings;

void RidingOutputs::reset_for_teleport() {
    const float elapsed = ground.time_without_wheel_contact;
    const auto drag = ground.wheel_angular_drag;
    ground = BoardGroundState{};
    ground.time_without_wheel_contact = elapsed;
    ground.wheel_angular_drag = drag;
    wheel_lines = WheelLineState{};
    probes.reset_results();
    // Original82C0D680 preserves query handles203C..2050. EndBoard
    // still consumes the batch submitted before this input-phase reset.
}

rs::Result<RidingOutputs, std::string> RidingOutputs::load(const sd::Collections& data,
                                                           const sc::physics::board_runtime::BoardRuntime& board,
                                                           std::uint32_t processed_flags_2468) {
    auto f = [&](const char* field) { return data.float_("physics_reckoning", "default", field); };
    auto control = [&](const char* field) -> rs::Result<std::array<float, 4>, std::string> {
        S3G_TRY(const auto w, data.words<4>("physics_reckoning", "default", field));
        return rs::from_bits(w);
    };
    auto curve = [&](const char* field) { return graph(data, "physics_reckoning", field); };
    S3G_TRY(const auto deck_words, data.words<20>("physics_reckoning", "default", "DeckAngleUsageVsSpeed"));
    const auto deck_curve = rs::from_bits(deck_words);
    GroundOrientationSettings orientation_settings{};
    S3G_TRY(orientation_settings.ground_normal_smoothing, control("GroundNormalSmoothing"));
    S3G_TRY(orientation_settings.up_vector_smoothing_slow, control("UpVectorSmoothingSlow"));
    S3G_TRY(orientation_settings.up_vector_smoothing_fast, control("UpVectorSmoothingFast"));
    S3G_TRY(orientation_settings.dynamic_up_vs_ground_y, curve("DynamicUpVsGroundY"));
    S3G_TRY(orientation_settings.ground_vector_blend, curve("GroundVectorBlendGraph"));
    for (std::size_t i = 0; i < 8; ++i) {
        orientation_settings.deck_angle_usage_vs_speed.x[i] = deck_curve[4 + i];
        orientation_settings.deck_angle_usage_vs_speed.y[i] = deck_curve[12 + i];
    }
    S3G_TRY(orientation_settings.up_vector_smoothing_vs_speed, curve("UpVectorSmoothingVsSpeed"));
    S3G_TRY(orientation_settings.up_vector_max_delta_vs_speed, curve("UpVectorMaxDeltaVsSpeed"));
    S3G_TRY(orientation_settings.ground_blend_max_delta, f("GroundVMaxBlendDelta"));
    S3G_TRY(orientation_settings.up_vector_max_acceleration, f("UpVectorMaxAcceleration"));
    S3G_TRY(orientation_settings.anti_wobble_damping, f("UpVectAntiWobbleDamping"));
    S3G_TRY(orientation_settings.extra_side_damping, f("ExtraSideDamping"));
    S3G_TRY(const auto min_wheels, data.integer("physics_reckoning", "default", "MinWheelsToUseGroundVector"));
    orientation_settings.minimum_wheels_for_ground_blend = static_cast<std::int32_t>(min_wheels);
    SpeedAndSlopeSettings speed_settings{};
    S3G_TRY(speed_settings.turn_torque_vs_speed, graph(data, "physics_heading", "TurnTorqueVsSpeed"));
    S3G_TRY(speed_settings.turn_torque_vs_slope, graph(data, "physics_heading", "TurnTorqueVsSlope"));
    S3G_TRY(speed_settings.heading_adjust_max_speed, data.float_("physics_heading", "default", "HeadingAdjustMaxSpeed"));
    const auto reckoning = GroundOrientation::new_(orientation_settings);
    const auto motion = BoardMotionOutput::from_board(board, reckoning.ground_normal, processed_flags_2468);
    const auto ground = BoardGroundState{};
    const float heading_adjust_factor = speed_settings.calculate(ground.wheel_normal.y, motion.forward_speed);
    RidingOutputs out{};
    out.ground = ground;
    out.wheel_lines = WheelLineState{};
    out.probes = BoardProbes{};
    out.reckoning = reckoning;
    out.reckoning_frames = ReckoningFrames::new_();
    out.body_spin = sc::air::body_spin::BodySpinState::new_();
    out.motion = motion;
    out.orientation_settings = orientation_settings;
    S3G_TRY(out.tilt_vs_rotation, curve("TiltVsRotGround"));
    S3G_TRY(out.tilt_vs_slope, curve("TiltVsSlopeGround"));
    out.speed_settings = speed_settings;
    S3G_TRY(out.maximum_ground_angle, f("MaxAllowedGroundNormalFromUp"));
    out.heading_adjust_factor_ = heading_adjust_factor;
    out.pending_wheel_queries = std::nullopt;
    return out;
}

float RidingOutputs::update_input_heading(float normal_y, float speed) {
    heading_adjust_factor_ = speed_settings.calculate(normal_y, speed);
    return heading_adjust_factor_;
}

void RidingOutputs::update_ground_reckoning(const sc::physics::board_runtime::BoardRuntime& board,
                                            RidingPoseInputs pose, std::uint32_t processed_flags_2468,
                                            float animation_balance, bool coffin,
                                            const sc::player::input_phase::ProcessedPhysicsInput& processed) {
    const auto deck = board.part_transforms()[sc::physics::board::index(BodyId::Deck)];
    // Ordinary ground riding follows the physical deck (82D4E250).
    update_ground_reckoning_with_heading(
        board, pose, processed_flags_2468, animation_balance, coffin, processed,
        {deck.basis.columns[2][0], deck.basis.columns[2][1], deck.basis.columns[2][2], 0.0f});
}

void RidingOutputs::update_ground_reckoning_with_heading(
    const sc::physics::board_runtime::BoardRuntime& board, RidingPoseInputs pose, std::uint32_t processed_flags_2468,
    float animation_balance, bool coffin, const sc::player::input_phase::ProcessedPhysicsInput& processed,
    std::array<float, 4> heading) {
    const auto observations = ground_input::GroundPacketInputs::from_processed(processed);
    const auto deck = board.part_transforms()[sc::physics::board::index(BodyId::Deck)];
    // The source takes the previous final frame X for the damping step.
    const auto previous_right = reckoning_frames.system[0];
    reckoning_frames.heading = heading;
    sc::air::body_spin::update_ground(body_spin, pose.body_spin);
    const auto effective = BoardMotionOutput::from_board(board, reckoning.ground_normal, processed_flags_2468);
    GroundOrientationInput input{};
    input.com_to_deck = pose.com_to_deck;
    input.ground_normal = observations.wheel_normal;
    input.dynamic_up = observations.dynamic_up;
    input.speed = observations.speed;
    input.wheel_contact_count = observations.wheel_count;
    input.animation_balance = animation_balance;
    input.deck_angle_curve_input = observations.absolute_speed;
    input.board_up = vector(deck.basis.columns[1]);
    input.board_forward = vector(deck.basis.columns[2]);
    input.effective_board_forward = vector(effective.effective_basis.columns[2]);
    input.previous_reckoning_right = Vector3::new_(previous_right[0], previous_right[1], previous_right[2]);
    input.prevent_up_behind_board = coffin;
    reckoning.update(orientation_settings, input);
    const auto up = lanes(reckoning.up);
    reckoning_frames.calculate_transform(up, lanes(reckoning.ground_normal));
    reckoning_frames.calculate_dynamic_lean(up, lanes(reckoning.dynamic_up));
    reckoning_frames.calculate_tilt((processed_flags_2468 & 0x100000u) != 0, tilt_vs_rotation, tilt_vs_slope);
}

void RidingOutputs::update_slide_reckoning(const sc::player::input_phase::ProcessedPhysicsInput& p,
                                           const sc::physics::board_toolkit::BoardToolkit& toolkit,
                                           float physical_body_spin, float balance) {
    auto raw = [](std::array<std::uint32_t, 4> v) {
        const auto f = rs::from_bits(v);
        return Vector3::new_(f[0], f[1], f[2]);
    };
    auto xyz = [](std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); };
    const auto previous_right = reckoning_frames.system[0];
    reckoning_frames.heading = toolkit.deck[2];
    reckoning.dynamic_up = raw(p.vectors_464_480_496_512_528[4]);
    sc::air::body_spin::update_ground(body_spin, physical_body_spin);
    GroundOrientationInput input{};
    input.com_to_deck = raw(p.animation_com_to_deck_752);
    input.ground_normal = raw(p.vectors_464_480_496_512_528[0]);
    input.dynamic_up = raw(p.vectors_464_480_496_512_528[4]);
    input.speed = p.scalar_2652;
    input.wheel_contact_count = static_cast<std::int32_t>(p.wheel_count_2556);
    input.animation_balance = balance;
    input.deck_angle_curve_input = toolkit.absolute_speed;
    input.board_up = xyz(toolkit.deck[1]);
    input.board_forward = xyz(toolkit.deck[2]);
    input.effective_board_forward = xyz(toolkit.effective[2]);
    input.previous_reckoning_right = xyz(previous_right);
    input.prevent_up_behind_board = (p.flags_2476 & 0x4000'0000u) != 0;
    reckoning.update(orientation_settings, input);
    const auto up = lanes(reckoning.up);
    reckoning_frames.calculate_transform(up, lanes(reckoning.ground_normal));
    reckoning_frames.calculate_dynamic_lean(up, lanes(reckoning.dynamic_up));
    reckoning_frames.calculate_tilt((p.flags_2468 & 0x100000u) != 0, tilt_vs_rotation, tilt_vs_slope);
}

Status RidingOutputs::start_wheel_queries(const sc::physics::board_runtime::BoardRuntime& board,
                                          const sc::physics::board_world::BoardWorld& world) {
    if (pending_wheel_queries.has_value())
        return rs::make_err(std::string("Wheel queries were started twice without result publication"));
    const auto lines = sc::physics::board_ground::wheel_lines(board, reckoning.up);
    std::array<std::optional<WheelLineHit>, 4> results{};
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto r = world.query_thin_line(lines[i].start, lines[i].end);
        if (r.is_err()) return rs::make_err(fmt::cat("Wheel", i, " stock line query: ", r.unwrap_err()));
        if (const auto& hit = r.unwrap()) {
            WheelLineHit h{};
            h.fraction = hit->geometry.fraction;
            h.normal = hit->geometry.normal;
            h.surface_tag = hit->tag;
            results[i] = h;
        } else {
            results[i] = std::nullopt;
        }
    }
    S3G_CHECK(probes.start(board, world));
    pending_wheel_queries = results;
    return ok();
}

Status RidingOutputs::finish_wheel_queries() {
    if (!pending_wheel_queries.has_value())
        return rs::make_err(std::string("EndBoard requires its submitted wheel query batch"));
    const auto hits = *pending_wheel_queries;
    pending_wheel_queries = std::nullopt;
    wheel_lines.publish(hits);
    return probes.publish();
}

Status RidingOutputs::finish_post_physics(sc::physics::board_runtime::BoardRuntime& board, bool board_wiping_out,
                                          std::uint32_t processed_flags_2468, float time_step) {
    ground.update(board.contact_reports(), wheel_lines, reckoning.up, maximum_ground_angle, board_wiping_out);
    std::array<Vector3, sc::physics::board::BODY_COUNT> velocities{};
    for (std::size_t i = 0; i < velocities.size(); ++i) velocities[i] = board.bodies()[i].rates.linear_velocity;
    ground.sample_accelerations(velocities, time_step);
    ground.advance_contact_time(time_step);
    auto& bodies = board.bodies_mut();
    for (std::size_t i = 0; i < 4 && i < ground.wheel_angular_drag.size(); ++i) {
        // S3 82C08634 writes inertia+36; S2 82B372E0 names this
        // mAngularDrag. Linear drag belongs to separate state controls.
        bodies[i].inertia.angular_drag = ground.wheel_angular_drag[i];
    }
    motion = BoardMotionOutput::from_board(board, reckoning.ground_normal, processed_flags_2468);
    return ok();
}

sc::graph::conditions::SpeedInputs RidingOutputs::graph_speeds() const {
    return sc::graph::conditions::SpeedInputs{motion.ground_speed, motion.forward_speed, heading_adjust_factor_};
}

Vector3 vector(std::array<float, 3> v) { return Vector3::new_(v[0], v[1], v[2]); }

std::array<float, 4> lanes(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }

rs::Result<PointGraph<8>, std::string> graph(const sd::Collections& data, const std::string& class_,
                                             const std::string& field) {
    S3G_TRY(const auto w, data.words<16>(class_, "default", field));
    const auto words = rs::from_bits(w);
    PointGraph<8> g{};
    for (std::size_t i = 0; i < 8; ++i) {
        g.x[i] = words[i];
        g.y[i] = words[8 + i];
    }
    return g;
}

}  // namespace s3::game::physics::riding_outputs
