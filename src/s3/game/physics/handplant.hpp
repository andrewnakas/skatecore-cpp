// Port of skate-game/src/physics/handplant.rs
//! TU3 HandPlantManager82D61040 and physical state600 (823273CC).
#pragma once
#include <array>
#include <cfloat>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/slice.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/handplant/contact.hpp"
#include "s3/game/physics/handplant/ik.hpp"
#include "s3/game/physics/handplant/rotation.hpp"
#include "s3/game/physics/handplant/settings.hpp"
#include "s3/game/physics/handplant/trajectory.hpp"

namespace s3::game::physics::handplant {

// use super::footplant::math; use math::*;  (group C: s3/game/physics/footplant/math.hpp; used in .cpp files)
using settings::Settings;
using ::s3::air::trajectory::Trajectory;

using V = std::array<float, 4>;
inline constexpr V UP = {0.0f, 1.0f, 0.0f, 0.0f};
inline constexpr V GRAVITY = {0.0f, rs::from_bits(0xc11ccccdu), 0.0f, 0.0f};
inline const Trajectory EMPTY = Trajectory{{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, -1.0f};

struct Handplant {
    Settings settings{};  // private in Rust
    std::uint32_t flags = 0;
    float phase = FLT_MAX;
    V anchor{};
    V previous_candidate_point{};  //2192; separate from the published anchor1856.
    std::optional<std::tuple<contact::Candidate, V, V, V, V>> pending{};
    std::optional<contact::Candidate> candidate{};
    Trajectory initial = EMPTY;
    Trajectory entry = EMPTY;
    std::array<Trajectory, 2> outgoing = {EMPTY, EMPTY};
    std::array<V, 4> curve{};
    std::array<std::array<V, 4>, 4> rotations = {::s3::physics::skeleton_animation_record::IDENTITY,
                                                 ::s3::physics::skeleton_animation_record::IDENTITY,
                                                 ::s3::physics::skeleton_animation_record::IDENTITY,
                                                 ::s3::physics::skeleton_animation_record::IDENTITY};
    V direction{};
    float travel_sign = 0.0f;
    float elapsed = -1.0f;
    float warped = -1.0f;
    float apex = -1.0f;
    float estimated_phase = -1.0f;  //2944; retained when the authored mean is zero.
    float out_duration = 0.0f;
    bool continuation = true;
    std::int32_t direction_hint = 0;
    std::int32_t direction_count = 0;
    float ik_blend = 0.0f;
    float ik_distance = -1.0f;
    bool ik_released = false;
    bool ik_latched = false;

    static rs::Result<Handplant, std::string> load(const sd::Collections& data);
    std::array<float, 3> animation_thresholds() const;
    ///82D62F20 preserves the new-position bit and the stored trajectories.
    void reset();
    void full_reset();
    void reset_ik();       // private in Rust
    void estimate_apex();  // private in Rust

    // ---- impl Handplant in handplant/trajectory.rs ----
    void launch(contact::Candidate c, V com, V velocity, V normal, V body_heading, V heading);
    Status select_outgoing(const ::s3::physics::board_world::BoardWorld& world);
    void build_curve();  // private in Rust
    std::tuple<V, V, V> values(::s3::physics::Slice<const std::array<V, 4>> pose, V physical_com);
};

float apex_time(Trajectory t);

///Ground82D37F38 consumes the candidate query before its shared skeleton update.
Status ground_update(GamePhysics& physics, SkaterRuntime& skater);

/// Capture the solved pose before the bail check, rather than comparing this
/// tick's targets against last tick's bodies in HANDPLANT_POSE.
void trace_solved(const GamePhysics& physics, const SkaterRuntime& skater);

///Ground82D38430 submits the candidate before82D37F38 consumes its investigation.
void ground_query(const GamePhysics& physics, SkaterRuntime& skater);

Status enter(GamePhysics& physics, SkaterRuntime& skater);

Status update(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::handplant
