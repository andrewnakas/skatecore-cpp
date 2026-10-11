// Port of skate-game/src/physics/ground_runtime/services.rs.
//! Ground's physical operations execute against the live board, in core's
//! recovered order. Detached queue/inertia ownership is published before the
//! solver runs; no force, drag or steering output is left in a test-only object.
//! (`impl GroundRuntime::update_board` is declared in GroundRuntime; the private
//! `BoardServices` adapter lives in services.cpp.)
#pragma once
#include <array>
#include <functional>
#include <optional>

#include "s3/core/math.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/ground_hang_geometry.hpp"
#include "s3/core/physics/manual/state.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/wipeout.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/riding/ground_contact_response.hpp"
#include "s3/core/riding/speed_model.hpp"
#include "s3/core/riding/speed_wobble.hpp"
#include "s3/core/riding/steering.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/ground_runtime/launch.hpp"

namespace s3::game::physics::ground_runtime::services {

using launch::GroundLaunchInfo;
using launch::GroundLaunchPhysical;

struct GroundControllers {
    sc::riding::speed_wobble::SpeedWobbleState& speed_wobble;
    sc::riding::steering::TruckSteeringState& truck_steering;
    sc::riding::speed_model::SpeedModelState& speed_model;
    sc::physics::manual::state::ManualState& manual;
    float& heading_previous;
};

struct GroundPhysicalFrame {
    const sc::physics::skeleton_animation_record::SkeletonAnimationRecord& skeleton_record;
    sc::riding::ground_contact_response::WallRidePhysical wall_ride;
    sc::riding::collision_response::CollisionResponsePhysical collision;
    sc::physics::ground_hang_geometry::HangGeometryInput hang_geometry;
    std::array<float, 4>& processed_velocity;
    ///The same material record used by colliders::world_volumes for all4wheels.
    sc::physics::contact::RetailContactMaterial& wheel_material;
    sc::player::wipeout::Requests& wipeout;
    float time_step = 0.0f;
    ///Needed only when the native wall-jump branch is selected. Missing data
    ///is an error on that branch, never an invented physical skeleton pose.
    std::optional<GroundLaunchPhysical> launch_physical;
    ///Own host call boundary for the full native TrajectorySelector Launch
    ///and Update. Callers must finish prediction before advancing this board.
    /// (Rust `&mut dyn FnMut(&GroundLaunchInfo) -> Result<(), String>`.)
    std::function<Status(const GroundLaunchInfo&)> launch_and_update;
};

/// Private helper.
std::array<float, 4> vector(sc::math::Vector3 v);

}  // namespace s3::game::physics::ground_runtime::services
