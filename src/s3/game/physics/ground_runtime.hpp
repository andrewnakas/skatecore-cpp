// Port of skate-game/src/physics/ground_runtime/mod.rs.
//! Owned native ground contact/force services for the live BoardRuntime.
//! The coordinator supplies the processed physical packet in native phase order.
#pragma once
#include <array>
#include <optional>
#include <string>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/ground_hang_geometry.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/riding/ground_contact_response.hpp"
#include "s3/core/riding/grounded/state.hpp"
#include "s3/core/riding/grounded/state/board.hpp"
#include "s3/core/riding/grounded/state/board_types.hpp"
#include "s3/core/riding/pumping/geometry.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/ground_runtime/corrections.hpp"
#include "s3/game/physics/ground_runtime/entry.hpp"
#include "s3/game/physics/ground_runtime/input.hpp"
#include "s3/game/physics/ground_runtime/launch.hpp"
#include "s3/game/physics/ground_runtime/pumping.hpp"
#include "s3/game/physics/ground_runtime/services.hpp"
#include "s3/game/physics/ground_runtime/settings.hpp"
#include "s3/game/physics/ground_runtime/state.hpp"
#include "s3/game/physics/ground_runtime/surface.hpp"
#include "s3/game/physics/ground_runtime/update.hpp"

namespace s3::game::physics::ground_runtime {

using entry::GroundEntryTargets;
using state::GroundState;
using update::GroundUpdateFrame;
using update::GroundUpdateTargets;
using input::GroundInputObservations;
using pumping::GroundPumping;
using launch::GroundLaunchInfo;
using launch::GroundLaunchPhysical;
using services::GroundControllers;
using services::GroundPhysicalFrame;
using settings::GroundProfiles;
using settings::GroundSettings;
using surface::active_audio_surface;
using surface::active_surface;
using surface::surface_key;

using sc::riding::grounded::state::board_types::CollisionForceResponse;
using sc::riding::grounded::state::board_types::GroundContactFrame;
using sc::riding::grounded::state::board_types::GroundContactResponse;

struct GroundRuntime {
    std::array<float, 4> retained_board_normal{};
    sc::riding::ground_contact_response::WallRideSettings wall_ride;
    sc::riding::collision_response::CollisionResponseSettings collision;
    float deck_center_to_truck = 0.0f;
    float launch_cone_x = 0.0f;
    float launch_cone_z = 0.0f;
    GroundContactResponse contact;
    std::optional<CollisionForceResponse> collision_force;

    /// Skateboard Reset82C060B0 publishes [0,1,0,0] to the retained
    /// toolkit normal192. Do not reset Ground contact-response state here.
    void reset_board_toolkit();
    static rs::Result<GroundRuntime, std::string> load(const sd::Collections& data);
    /// Slide82D3AA74..AA9C passes a fresh zero previous vector each update.
    /// This call does not overwrite Ground's separately retained contact state.
    GroundContactResponse contact_response_with_previous(GroundContactFrame frame,
                                                         sc::riding::ground_contact_response::WallRidePhysical physical,
                                                         std::array<float, 4> previous) const;
    /// Called immediately after contact response and before animated/physical
    /// branch selection at82D389DC. This changes the solver's actual deck.
    void update_body_accumulator(sc::physics::board_runtime::BoardRuntime& board);
    /// Native82D944E8 output publication. Do not discard an existing ground
    /// collision vector on a false return; caller's collision-fade state owns it.
    std::optional<CollisionForceResponse> calculate_collision_force(
        sc::riding::collision_response::CollisionResponsePhysical physical);
    void apply_angular_target(sc::physics::board_runtime::BoardRuntime& board, std::array<float, 4> displacement);
    void apply_angular_displacement(sc::physics::board_runtime::BoardRuntime& board,
                                    std::array<float, 4> displacement);
    ///82C04168 publishes the requested linear velocity to all seven real body
    /// parts, preserving angular velocity and accumulated forces.
    void set_animated_velocity(sc::physics::board_runtime::BoardRuntime& board, std::array<float, 4> velocity);

    // ---- corrections.rs ----
    /// `impl ManualAngleMeasurement for GroundRuntime` and `impl AntiFlipNudgeMath for GroundRuntime`
    /// (both `type Error = Infallible`).
    using Error = sc::riding::pumping::geometry::Infallible;
    rs::Result<float, Error> angle_between(std::array<float, 4> deck_z, std::array<float, 4> reference_z,
                                           std::array<float, 4> reference_x);
    rs::Result<float, Error> dot3(std::array<float, 4> a, std::array<float, 4> b);
    rs::Result<std::array<float, 4>, Error> scale_to_magnitude(std::array<float, 4> vector, float squared_length,
                                                               float magnitude);
    rs::Result<bool, std::string> detect_hung_up_geometry(
        const sc::physics::board_world::BoardWorld& world,
        sc::physics::ground_hang_geometry::HangGeometryInput input) const;
    float collision_force_projection(std::array<float, 4> force, std::array<float, 4> velocity) const;
    std::array<float, 4> build_hang_force(const sc::physics::board_runtime::BoardRuntime& board,
                                          std::array<float, 4> edge_start, std::array<float, 4> edge_end) const;
    void apply_hang_force(sc::physics::board_runtime::BoardRuntime& board, std::array<float, 4> force);
    void pin_to_position(sc::physics::board_runtime::BoardRuntime& board, float captured_x, float captured_z,
                         float dt);

    // ---- services.rs ----
    rs::Result<sc::riding::grounded::state::board::GroundBoardOutcome, std::string> update_board(
        sc::physics::board_runtime::BoardRuntime& board, const sc::physics::board_world::BoardWorld& world,
        const GroundSettings& settings, sc::riding::grounded::state::PhysicsGroundState& state,
        GroundControllers controllers, sc::riding::grounded::state::board_types::GroundBoardInput input,
        GroundPhysicalFrame physical);

    // ---- input.rs ----
    /// Input preparation before Ground. Retained normal has its own native
    /// lifetime and is updated here, not in post-solver contact publication.
    sc::physics::board_toolkit::BoardToolkit prepare_toolkit(const sc::physics::board_runtime::BoardRuntime& board,
                                                             const sc::player::input_phase::ProcessedPhysicsInput& p);
};

/// Private helpers.
rs::Result<sc::point_graph::PointGraph<8>, std::string> curve(const sd::Collections& data, const char* class_,
                                                              const char* field);
sc::math::Vector3 xyz(std::array<float, 4> v);

}  // namespace s3::game::physics::ground_runtime
