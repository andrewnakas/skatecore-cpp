// Port of skate-game/src/physics/offboard/skeleton_ground/state.rs
//! Persistent animation-space board16016 and the Biped reckoning callback.
#pragma once
#include <array>
#include <string>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/offboard/ground_reckoning.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/physics/offboard/skeleton_ground.hpp"

namespace s3::game::physics::air_reckoning {
struct AirReckoning;  // physics/air_reckoning.hpp (group B)
}
namespace s3::game::physics::riding_outputs {
struct RidingOutputs;  // physics/riding_outputs.hpp (group D)
}

namespace s3::game::physics::offboard::skeleton_ground::state {

using sc::point_graph::PointGraph;

struct State {
    //82BDE158..17C copies mapped12624 here only when2484bit0 is clear.
    //This must never alias physical_board12496 or be reset on each update.
    // pub(super)
    Transform retained_board = sc::physics::skeleton_animation_record::IDENTITY;
    // Private in Rust.
    std::array<float, 4> ground_normal_smoothing{};
    PointGraph<8> tilt_vs_rotation{};
    PointGraph<8> tilt_vs_slope{};

    static rs::Result<State, std::string> load(const sd::Collections& data);
    ///Apply after GeneralUpdate and trajectory clear, replacing the riding job.
    ///82BDE2CC explicitly passes r7=1: consume physical body spin2812.
    sc::player::offboard::ground_reckoning::Output finish_reckoning(
        ReckoningUpdate update, riding_outputs::RidingOutputs& riding, air_reckoning::AirReckoning& air,
        const sc::player::input_phase::ProcessedPhysicsInput& processed, float physical_body_spin) const;
};

}  // namespace s3::game::physics::offboard::skeleton_ground::state
