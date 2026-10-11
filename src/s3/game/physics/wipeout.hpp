// Port of skate-game/src/physics/wipeout.rs
//! Shared Ground/Air wipeout requests from original TU3 82D8F9E0/82D90358.
#pragma once
#include <array>
#include <string>

#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/wipeout.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/wipeout/observations.hpp"
#include "s3/game/physics/wipeout/settings.hpp"

namespace s3::game::physics::wipeout {

// pub(crate) use observations::Observations;
using observations::Observations;

struct Wipeout {
    ::s3::player::wipeout::Requests state{};
    // Private in Rust; accessed by check_after_physics in this module.
    ::s3::player::wipeout::Settings settings{};
    std::array<::s3::player::wipeout::Mode, 5> modes{};

    Status check_air_collision(const ::s3::player::input_phase::ProcessedPhysicsInput& p,
                               const ::s3::player::wipeout::Frame& frame);
    static rs::Result<Wipeout, std::string> load(const sd::Collections& data);
    Status check_ground(const Observations& input);
    Status check_ground_animation(const Observations& input, float scale);
    Status check_air(const Observations& input, bool use_com);
    rs::Result<::s3::player::wipeout::Mode, std::string> mode(
        const ::s3::player::input_phase::ProcessedPhysicsInput& p) const;
    ///Consumed by state selection and by the same tick's postphysics IK branch.
    bool requests_runout(const ::s3::player::input_phase::ProcessedPhysicsInput& p) const;
    bool requests_wipeout(const ::s3::player::input_phase::ProcessedPhysicsInput& p) const;
};

///Player postphysics82BD83E0 follows contact/error publication with the
///selected state's check, before FootIK sees that tick's wipeout request.
Status check_after_physics(const GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::wipeout
