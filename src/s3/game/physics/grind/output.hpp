// Port of skate-game/src/physics/grind/output.rs.
//! Common82D40EA0 and six family FillOut overrides. Post/selector are separate.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/player/input_phase/grind_output.hpp"
#include "s3/core/rs.hpp"
#include "s3/game/physics/grind/family.hpp"
#include "s3/game/physics/grind/observation.hpp"

namespace s3::game::physics::grind::output {

using observation::ManagerObservation;
using ::s3::player::input_phase::grind_output::GrindOutputFields;
using V = std::array<float, 4>;

/// Retained physical-state fields, not reconstructed from filtered output.
struct State {
    V direction{};
    V normal{};
    V across{};
    float crouch = 0.0f;
    bool leaving = false;
    std::uint32_t substate = 0;
    std::uint32_t classification_104 = 0;
    bool just_jumped = false;
    V jump_velocity{};
    bool slide_wipeout = false;
    V slide_impulse{};
    std::array<bool, 3> tipslide_97_98_99{};
};

/// Fields owned by these FillOut methods. The caller retains this between
/// publications: unwritten optional fields must survive exactly as in S3.
struct PhysicalOutput {
    V direction{};
    V point{};
    V normal{};
    V across{};
    std::array<V, 2> primitive_endpoints{};
    V high_side{};
    float impact_speed = 0.0f;
    float crouch = 0.0f;
    Family family = Family::FiftyFifty;
    std::uint32_t substate = 0;
    std::uint32_t audio_surface = 0;
    std::array<std::uint64_t, 2> spline_guids{};
    std::uint32_t classification = 0;
    bool leaving = false;
    bool on_front = false;
    bool is_ledge = false;
    bool curb = false;
    std::array<bool, 3> tipslide_324_325_326{};
};

/// Optional writes to OTHER physical output blocks. No inactive-event clearing
/// is performed here; the owner of Air/Wipeout output owns those resets.
struct SideEffects {
    std::optional<V> air_jump_velocity_128;
    std::optional<V> wipeout_impulse_16;
};
S3_DERIVE_EQ(SideEffects, air_jump_velocity_128, wipeout_impulse_16)

SideEffects fill(Family family, const State& state, const ManagerObservation& manager, PhysicalOutput& output);

/// Adapt the retained native publication directly. Unwritten family fields,
/// camera state, graph names and selector flags are not reset by FillOut.
SideEffects fill_fields(Family family, const State& state, const ManagerObservation& manager,
                        GrindOutputFields& out);

}  // namespace s3::game::physics::grind::output
