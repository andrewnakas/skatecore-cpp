// Port of skate-game/src/physics/biped_ground.rs
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>

#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/offboard/air_launch.hpp"
#include "s3/core/player/offboard/contact_toolkit.hpp"
#include "s3/core/player/offboard/controller.hpp"
#include "s3/core/player/offboard/ground_entry.hpp"
#include "s3/core/player/offboard/ground_job.hpp"
#include "s3/core/player/offboard/ground_lifecycle.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/core/player/offboard/ground_reckoning.hpp"
#include "s3/core/player/offboard/ground_sync.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/animation_metadata.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/biped_ground/entry.hpp"
#include "s3/game/physics/biped_ground/feet.hpp"
#include "s3/game/physics/biped_ground/grab.hpp"
#include "s3/game/physics/biped_ground/grab_runtime.hpp"
#include "s3/game/physics/biped_ground/ground_board.hpp"
#include "s3/game/physics/biped_ground/lifecycle.hpp"
#include "s3/game/physics/biped_ground/services.hpp"
#include "s3/game/physics/biped_ground/sync.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/offboard/ground_geometry.hpp"  // group A (offboard)
#include "s3/game/physics/offboard/skeleton_ground.hpp"  // group A (offboard)

namespace s3::game::physics::riding_outputs {
struct RidingOutputs;  // physics/riding_outputs.hpp (group D)
}
namespace s3::game::physics::air_reckoning {
struct AirReckoning;  // physics/air_reckoning.hpp (group B)
}

namespace s3::game::physics::biped_ground {

// pub(crate) use lifecycle::{fill, post, publish_fields};
using lifecycle::fill;
using lifecycle::post;
using lifecycle::publish_fields;

namespace contact_toolkit = ::s3::player::offboard::contact_toolkit;
namespace ground_entry = ::s3::player::offboard::ground_entry;
namespace ground_job = ::s3::player::offboard::ground_job;

struct Owner {
    ::s3::player::offboard::controller::Controller controller{};
    std::optional<::s3::player::offboard::controller::GroundResult> result{};
    ground_entry::State ground{};
    contact_toolkit::ContactPrefix contact{};
    std::optional<::s3::player::offboard::ground_query::GroundAdjustment> geometry_adjustment{};
    ::s3::game::physics::offboard::skeleton_ground::State skeleton_state;  // private in Rust
    ::s3::game::physics::offboard::ground_geometry::State geometry;        // private in Rust
    ::s3::point_graph::PointGraph<8> movement_vs_stick_angle{};             // private in Rust
    ::s3::point_graph::PointGraph<8> turn_vs_stick_angle{};                 // private in Rust
    ::s3::player::offboard::air_launch::Settings air_launch{};              // private in Rust
    ::s3::player::offboard::ground_lifecycle::CollisionSettings collision_settings{};  // private in Rust
    ::s3::player::offboard::ground_sync::BoardSettings grab_settings{};

    ///Only the shared82D8E3E0 callee, not Ground movement/lifecycle.
    ///Ground82BDE2CC and Air82BDDFFC both supply enable-body-spin r7=1.
    ///Each caller supplies its own native up/forward/blend and phase ordering.
    ::s3::player::offboard::ground_reckoning::Output finish_reckoning(
        ::s3::game::physics::offboard::skeleton_ground::ReckoningUpdate update,
        riding_outputs::RidingOutputs& riding, air_reckoning::AirReckoning& air,
        const ::s3::player::input_phase::ProcessedPhysicsInput& processed, float physical_body_spin) const;

    std::tuple<::s3::point_graph::PointGraph<8>, ::s3::player::offboard::air_launch::Settings> air_settings() const;

    ::s3::point_graph::PointGraph<8> turn_vs_speed() const;  // private in Rust

    static rs::Result<Owner, std::string> load(const sd::Collections& data, const sd::AnimationMetadata& metadata);

    void reset(contact_toolkit::Owner& toolkit);

    ::s3::player::offboard::controller::GroundResult run(::s3::player::offboard::controller::GroundJob job);

    void enter(ground_entry::Input input, std::uint32_t current_state, std::array<std::array<float, 4>, 4> previous_frame,
               contact_toolkit::Owner& toolkit);

    // ---- impl Owner in biped_ground/sync.rs ----
    ///82D32338. This writes state146 on every call, including the no-launch path.
    rs::Result<std::optional<::s3::player::offboard::air_launch::Packet>, std::string> prepare_air(
        const sync::LaunchInput& input);
};

Status enter(GamePhysics& physics, SkaterRuntime& skater);

void exit(SkaterRuntime& skater);

rs::Result<contact_toolkit::Input, std::string> update(GamePhysics& physics, SkaterRuntime& skater,
                                                        const PlayerControls& controls,
                                                        ground_job::ContactSnapshot contact);

/// Call immediately AFTER submitting update's toolkit input through the shared
/// owner. The seven real line results become visible at the NEXT PreUpdate.
Status submit_geometry(const GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::biped_ground
