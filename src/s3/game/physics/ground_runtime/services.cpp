// Port of skate-game/src/physics/ground_runtime/services.rs.
//! Ground's physical operations execute against the live board, in core's
//! recovered order. Detached queue/inertia ownership is published before the
//! solver runs; no force, drag or steering output is left in a test-only object.
#include "s3/game/physics/ground_runtime/services.hpp"

#include <variant>

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/core/riding/grounded/drag.hpp"
#include "s3/core/riding/grounded/state/board.hpp"
#include "s3/game/physics/ground_runtime.hpp"

namespace s3::game::physics::ground_runtime {

namespace board_types = sc::riding::grounded::state::board_types;
namespace gcm = sc::riding::ground_correction_math;
using sc::physics::board_runtime::BoardRuntime;
using sc::physics::board_world::BoardWorld;
using services::GroundControllers;
using services::GroundPhysicalFrame;
using services::vector;

namespace {

/// Private `struct BoardServices<'a, 'p>` and its trait impls (ManualAngleMeasurement,
/// AntiFlipNudgeMath, HangUpServices, HalfpipeWheelCatchServices, PinningServices,
/// GroundBoardServices), all with `Error = String`.
struct BoardServices {
    GroundRuntime& runtime;
    BoardRuntime& board;
    const BoardWorld& world;
    const GroundSettings& settings;
    GroundPhysicalFrame& physical;
    std::optional<GroundLaunchInfo> launch;

    using Error = std::string;
    using BoardError = std::string;
    using AnimatedPose = GroundLaunchInfo;
    using Unit = rs::Result<rs::Unit, std::string>;

    // ---- ManualAngleMeasurement ----
    rs::Result<float, std::string> angle_between(std::array<float, 4> a, std::array<float, 4> b,
                                                 std::array<float, 4> axis) {
        return sc::riding::collision_response::signed_angle(xyz(a), xyz(b), xyz(axis));
    }
    // ---- AntiFlipNudgeMath ----
    rs::Result<float, std::string> dot3(std::array<float, 4> a, std::array<float, 4> b) {
        return gcm::dot_product(a, b);
    }
    rs::Result<std::array<float, 4>, std::string> scale_to_magnitude(std::array<float, 4> v, float square,
                                                                     float size) {
        return gcm::scale_to_magnitude(v, square, size);
    }
    // ---- HangUpServices ----
    rs::Result<std::array<float, 4>, std::string> build_hang_force() {
        const auto g = physical.hang_geometry;
        return runtime.build_hang_force(board, vector(g.edge_start), vector(g.edge_end));
    }
    Unit apply_hang_force(std::array<float, 4> f) {
        runtime.apply_hang_force(board, f);
        return rs::Unit{};
    }
    rs::Result<bool, std::string> detect_hung_up_geometry() {
        return runtime.detect_hung_up_geometry(world, physical.hang_geometry);
    }
    Unit request_wipeout() {
        //82D3983C..54 writes byte32 (reason12), value104 and increments200.
        physical.wipeout.request(12, 0.0f);
        return rs::Unit{};
    }
    // ---- HalfpipeWheelCatchServices ----
    rs::Result<std::array<float, 4>, std::string> angular_displacement() {
        const auto f = board.part_transforms()[sc::physics::board::index(sc::physics::board::BodyId::Deck)]
                           .basis.columns;
        return gcm::wheel_catch_displacement({f[1][0], f[1][1], f[1][2], 0.0f}, {f[2][0], f[2][1], f[2][2], 0.0f});
    }
    Unit apply_angular_displacement(std::array<float, 4> v) {
        runtime.apply_angular_displacement(board, v);
        return rs::Unit{};
    }
    // ---- PinningServices ----
    Unit pin_to_captured_position(float x, float z) {
        runtime.pin_to_position(board, x, z, physical.time_step);
        return rs::Unit{};
    }
    // ---- GroundBoardServices ----
    rs::Result<float, std::string> center_of_mass_height_82d38838() {
        return gcm::center_of_mass_height(physical.skeleton_record.com_to_deck_world);
    }
    Unit set_contact_wheel_materials() {
        physical.wheel_material = settings.wheel_material;
        return rs::Unit{};
    }
    rs::Result<GroundContactResponse, std::string> contact_response_82d93df0(GroundContactFrame frame,
                                                                             std::array<float, 4> previous) {
        runtime.contact = sc::riding::ground_contact_response::wall_ride_response(runtime.wall_ride,
                                                                                   physical.wall_ride, frame, previous);
        return runtime.contact;
    }
    Unit update_body_accumulator_82d389dc() {
        runtime.update_body_accumulator(board);
        return rs::Unit{};
    }
    Unit set_animated_velocity_82c04168(std::array<float, 4> v) {
        runtime.set_animated_velocity(board, v);
        return rs::Unit{};
    }
    Unit write_processed_velocity_400(std::array<float, 4> v) {
        physical.processed_velocity = v;
        return rs::Unit{};
    }
    rs::Result<GroundLaunchInfo, std::string> build_animated_pose_82d33448() { return GroundLaunchInfo{}; }
    Unit publish_animated_pose_82be33d0(const GroundLaunchInfo& info_) {
        if (!physical.launch_physical) {
            return rs::make_err(
                std::string("Ground wall jump requires actual Skeleton/Reckoning launch observations82BE33D0"));
        }
        GroundLaunchInfo info = info_;
        info.fill(*physical.launch_physical, runtime.launch_cone_x, runtime.launch_cone_z);
        launch = info;
        return rs::Unit{};
    }
    Unit update_external_player_82d67848(const GroundLaunchInfo& /*info*/, std::array<float, 4> velocity) {
        if (!launch) return rs::make_err(std::string("Ground launch packet was not filled"));
        launch->wall_jump(velocity);
        return rs::Unit{};
    }
    Unit commit_external_player_82d68800() {
        if (!launch) return rs::make_err(std::string("Ground launch packet was not prepared"));
        return physical.launch_and_update(*launch);
    }
    Unit finalize_animated_board_sk83_na_f_01a4() {
        //TU382B61BB8 is one blr instruction. This confirmed native no-op
        //does not replace the preceding required trajectory calculation.
        return rs::Unit{};
    }
    rs::Result<std::optional<CollisionForceResponse>, std::string> collision_force_82d944e8(
        std::array<float, 4> normal) {
        auto p = physical.collision;
        p.ground_normal = normal;
        return runtime.calculate_collision_force(p);
    }
    rs::Result<float, std::string> collision_force_dot_velocity_82d38e18(std::array<float, 4> f,
                                                                         std::array<float, 4> v) {
        return runtime.collision_force_projection(f, v);
    }
    Unit apply_vector_82c07000(std::array<float, 4> v) {
        runtime.apply_angular_target(board, v);
        return rs::Unit{};
    }
    Unit apply_angular_displacement_82c075b8(std::array<float, 4> v) {
        runtime.apply_angular_displacement(board, v);
        return rs::Unit{};
    }
};

/// Rust `{:?}` of a `String`.
std::string debug_string(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out + "\"";
}

const char* stage_name(board_types::GroundBoardStage s) {
    using S = board_types::GroundBoardStage;
    switch (s) {
        case S::CenterOfMassHeight: return "CenterOfMassHeight";
        case S::SetWheelMaterials: return "SetWheelMaterials";
        case S::ContactResponse: return "ContactResponse";
        case S::UpdateBodyAccumulator: return "UpdateBodyAccumulator";
        case S::SetAnimatedVelocity: return "SetAnimatedVelocity";
        case S::WriteProcessedVelocity: return "WriteProcessedVelocity";
        case S::BuildAnimatedPose: return "BuildAnimatedPose";
        case S::PublishAnimatedPose: return "PublishAnimatedPose";
        case S::UpdateExternalPlayer: return "UpdateExternalPlayer";
        case S::CommitExternalPlayer: return "CommitExternalPlayer";
        case S::FinalizeAnimatedBoard: return "FinalizeAnimatedBoard";
        case S::CollisionForce: return "CollisionForce";
        case S::ApplyCollisionVector: return "ApplyCollisionVector";
        case S::CollisionProjection: return "CollisionProjection";
        case S::ManualEffect: return "ManualEffect";
        case S::PushForceSquared: return "PushForceSquared";
        case S::AntiFlipNudge: return "AntiFlipNudge";
        case S::ApplyCollisionDecay: return "ApplyCollisionDecay";
        case S::ApplyStraighten: return "ApplyStraighten";
        case S::ApplyHeading: return "ApplyHeading";
        case S::ApplyAntiFlip: return "ApplyAntiFlip";
        case S::ApplyManual: return "ApplyManual";
        case S::HangUps: return "HangUps";
        case S::HalfpipeWheelCatch: return "HalfpipeWheelCatch";
        case S::Pinning: return "Pinning";
        case S::StrongForceSquared: return "StrongForceSquared";
    }
    return "?";
}

/// Rust `{:?}` of `GroundBoardError<String>`.
std::string debug_error(const board_types::GroundBoardError<std::string>& e) {
    using E = board_types::GroundBoardError<std::string>;
    if (const auto* s = std::get_if<E::Service>(&e.value)) {
        return fmt::cat("Service { stage: ", stage_name(s->stage), ", source: ", debug_string(s->source), " }");
    }
    if (const auto* m = std::get_if<E::Manual>(&e.value)) {
        if (m->_0.value.index() == 0) return "Manual(Angle(IntegerConversionUnavailable))";
        return fmt::cat("Manual(Measurement(", debug_string(std::get<1>(m->_0.value)), "))");
    }
    const auto& d = std::get<E::Drag>(e.value);
    return fmt::cat("Drag(",
                    d._0 == sc::riding::grounded::drag::DragBindingError::PartOutsideAssembly ? "PartOutsideAssembly"
                                                                                               : "InertiaOutsideStorage",
                    ")");
}

}  // namespace

rs::Result<sc::riding::grounded::state::board::GroundBoardOutcome, std::string> GroundRuntime::update_board(
    BoardRuntime& board, const BoardWorld& world, const GroundSettings& settings,
    sc::riding::grounded::state::PhysicsGroundState& state, GroundControllers controllers,
    board_types::GroundBoardInput input, GroundPhysicalFrame physical) {
    input.speed_wobble.activation_threshold = settings.wobble_activation;
    input.speed_wobble.amplitude_multiplier = settings.wobble_amplitude;
    auto queue = std::move(board.forces_mut());
    board.forces_mut() = sc::physics::force_queue::BoardForceQueue{};
    std::array<sc::physics::rigid_body::RetailInertiaDynamics, 7> inertias{};
    for (std::size_t i = 0; i < 7; ++i) inertias[i] = board.bodies()[i].inertia;
    static const std::array<std::size_t, 7> PART_INDICES{0, 1, 2, 3, 4, 5, 6};
    sc::riding::grounded::drag::BodyInertias bindings{
        sc::physics::Slice<const std::size_t>(PART_INDICES.data(), PART_INDICES.size()),
        sc::physics::Slice<sc::physics::rigid_body::RetailInertiaDynamics>(inertias.data(), inertias.size()),
    };
    BoardServices services{*this, board, world, settings, physical, std::nullopt};
    auto result = sc::riding::grounded::state::board::update(
        state,
        board_types::GroundBoardComponents{
            controllers.speed_wobble,
            controllers.truck_steering,
            controllers.speed_model,
            controllers.manual,
            controllers.heading_previous,
            queue,
            bindings,
        },
        settings.board(), input, services);
    //Only SetLinearDrag changes the detached inertia records. Publish that
    //field alone: direct body torque/velocity operations above are retained.
    auto& bodies = services.board.bodies_mut();
    for (std::size_t i = 0; i < bodies.size() && i < inertias.size(); ++i) {
        bodies[i].inertia.linear_drag = inertias[i].linear_drag;
    }
    services.board.forces_mut() = queue;
    if (result.is_err()) {
        return rs::make_err(fmt::cat("Native Ground board update: ", debug_error(result.unwrap_err())));
    }
    return result.unwrap();
}

}  // namespace s3::game::physics::ground_runtime

namespace s3::game::physics::ground_runtime::services {

std::array<float, 4> vector(sc::math::Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }

}  // namespace s3::game::physics::ground_runtime::services
