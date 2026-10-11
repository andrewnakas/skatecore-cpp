// Port of skate-game/src/physics/foot_ik_queries.rs.
//! PhysicalPlayerHiLOD StartSkeletonLineTests82DB63C0 / result publication
//!82DB6580. These are radius-bearing trajectory tests from actual physical parts.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::foot_ik_queries {

/// `#[derive(Clone, Copy, Debug)]`.
struct SkeletonLineHit {
    /// Result0 and32, copied to the physical query record by82DB65E0/F0.
    std::array<float, 4> position{};
    std::array<float, 4> normal{};
    /// Result132 and collisionTime48>=0, not an arbitrary geometry-present flag.
    std::uint32_t surface = 0;
    bool hit = false;
    float collision_time = 0.0f;

    /// Private in Rust.
    static SkeletonLineHit pending_result() {
        // Valid test result initialized by8276C9D8: position0, normalY,
        //time-1 and surface132=0. A no-hit result still has this real record.
        SkeletonLineHit r;
        r.position = {0.0f, 0.0f, 0.0f, 0.0f};
        r.normal = {0.0f, 1.0f, 0.0f, 0.0f};
        r.surface = 0;
        r.hit = false;
        r.collision_time = -1.0f;
        return r;
    }
};

struct SkeletonLineTests {
    SkeletonLineHit hips;
    std::array<SkeletonLineHit, 2> feet{};
    /// The hips ray's origin, retained at 1632 beside its result. See
    /// [`PlayerInputState::hips_line_origin_1632`].
    std::array<float, 4> hips_origin{};

    /// EndSkeletonLineTests82DB6580 publishes the completed tests in the
    /// order hips, left toe, right toe. ProcessInput copies these retained
    /// records into Processed1056/960/1008 during its input phase.
    void publish(sc::player::input_phase::PlayerInputState& player) const;
};

rs::Result<SkeletonLineTests, std::string> query(const sc::physics::board_world::BoardWorld& world,
                                                 const sc::physics::skeleton_body::SkeletonBody& body);

/// Private in Rust.
rs::Result<SkeletonLineHit, std::string> query_trajectory(const sc::physics::board_world::BoardWorld& world,
                                                          std::array<float, 4> start, std::array<float, 4> velocity);

}  // namespace s3::game::physics::foot_ik_queries
