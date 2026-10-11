// Port of skate-core/src/physics/filtered_state.rs.
// Complete filtered physical-state update82DE5BA0, with reset82DE5588.
// This conditions the actual selected physics state; collision alone does not
// select that state. Skate2 named82E2D800 identifies fields; TU3 governs branches.
#pragma once
#include <cstdint>
#include <limits>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::physics::filtered_state {

using animation::output::attributes::AttributeName;
using animation::skeleton_input::name::encode;

enum class FilteredCategory : std::uint32_t {
    Invalid = 0,  // #[default]
    Ground = 1,
    Air = 2,
    Grind = 3,
    Wipeout = 4,
    Teleport = 5,
    Offboard = 6,
    OffboardAir = 7,
};

/// PhysOut.Grinds fields copied when the selected state is400..405. Names are
/// native five-word strings, never addresses or collection hashes.
struct GrindState {
    // Default values: Initializer82F8AF08 encodes NUL text82060799 to830C0D60.
    std::int32_t kind = -1;
    std::int32_t scorable_id = -1;
    AttributeName name = encode("");
    AttributeName scoring_name = encode("");
    bool on_front = false;
    float crouch = 0.0f;
    std::uint64_t pathed_guid = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t local_guid = std::numeric_limits<std::uint64_t>::max();

    static GrindState default_() { return GrindState{}; }
};
S3_DERIVE_EQ(GrindState, kind, scorable_id, name, scoring_name, on_front, crouch, pathed_guid,
             local_guid)

struct FilteredStateInput {
    /// Selected PhysOut.State category12 and state16, not filtered categories.
    std::int32_t physics_category = 0;
    std::int32_t physics_state = 0;
    /// PhysOut.Collision3477 and16, after actual collision publication.
    bool anything_in_contact = false;
    std::int32_t physics_surface_type = 0;
    /// PhysOut.Ground316, owned by ground-state wall-ride exit calculations.
    bool wall_ride_exit = false;
    /// PhysOut.Air443, owned by the trajectory/grind target decision.
    bool targeting_grind = false;
    /// PhysOut.OffBoard320 and315 respectively.
    bool offboard_has_landed = false;
    bool offboard_on_deck = false;
    GrindState grind;
    /// EmpiricalMeasurements82DE5858 updates conditioner196 before this call.
    float last_grind_distance = 0.0f;
};

struct FilteredStateOutput {
    FilteredCategory category = FilteredCategory::Invalid;
    FilteredCategory previous_category = FilteredCategory::Invalid;
    bool grinding = false;
    GrindState grind;
    float last_grind_distance = 0.0f;
};
S3_DERIVE_EQ(FilteredStateOutput, category, previous_category, grinding, grind,
             last_grind_distance)

struct FilteredState {
    FilteredCategory category = FilteredCategory::Invalid;
    FilteredCategory previous_category = FilteredCategory::Invalid;
    std::int32_t previous_physics_state = 0;
    std::int32_t air_count = 0;
    std::int32_t nonspecific_count = 0;
    std::int32_t nonspecific_collision_free_count = 0;
    std::int32_t nonspecific_collision_count = 0;
    std::int32_t frames_since_ground_stairs = 0;
    bool must_change = true;
    // Private in Rust (exposed for the ported tests).
    // Nongrind publication clears the output metadata, not these cached fields.
    GrindState cached_grind;

    static FilteredState default_() { return FilteredState{}; }
    void reset() { *this = default_(); }
    FilteredStateOutput update(FilteredStateInput input);
};

}  // namespace s3::physics::filtered_state
