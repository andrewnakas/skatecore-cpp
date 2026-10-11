// Port of skate-game/src/physics/grind/runtime.rs.
//! Host lifetime for the six native physical-state instances and chromosome.
#pragma once
#include <array>
#include <optional>
#include <string>

#include "s3/core/physics/filtered_state.hpp"
#include "s3/core/player/input_phase/grind_output.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/grind/family.hpp"
#include "s3/game/physics/grind/observation.hpp"
#include "s3/game/physics/grind/output.hpp"
#include "s3/game/physics/grind/rng.hpp"
#include "s3/game/physics/grind/settings.hpp"
#include "s3/game/physics/grind/state.hpp"
#include "s3/game/physics/grind_chromosome.hpp"

namespace s3::game::physics::grind::runtime {

using observation::ManagerObservation;
using ::s3::physics::filtered_state::GrindState;
using ::s3::player::input_phase::grind_output::GrindOutputFields;

struct Runtime {
    // pub(super) in Rust (read/written by the sibling grind modules).
    std::array<state::State, 6> states{};
    std::optional<Family> active;
    bool nonspecific_active = false;
    bool nonspecific_jumped = false;
    std::array<float, 4> nonspecific_jump_velocity{};
    settings::Settings settings;
    std::optional<ManagerObservation> manager;
    /// Exact optional Wipeout output16/flag32 write for main's shared output
    /// owner. CURRENT does not yet expose that block in PhysicalPlayerInput.
    std::optional<std::array<float, 4>> pending_wipeout_impulse;

    static rs::Result<Runtime, std::string> load(const sd::Collections& data);
    std::optional<Family> active_family() const { return active; }
    std::array<std::uint32_t, 3> take_orientation_noise();
    /// Call immediately after the real input manager finishes its Post phase.
    void observe(const ManagerObservation& manager);
    /// Physical FillOut only. Selector owns grinding316; output reset owns all
    /// other templates. Active family comes from lifecycle, not manager selection.
    std::optional<output::SideEffects> fill_physical(const ManagerObservation& manager,
                                                     GrindOutputFields& out) const;
    /// Run once per completed output publication, including nongrind frames.
    /// Ground/Air history is not restricted to the six physical grind states.
    Status condition_outputs(const grind_chromosome::Input& input, GrindOutputFields& out);
    ///82DE5588/82DE5608 initializes196 to0;82DE5BA0/82DE5E10 copies it.
    ///82DE5858 measures velocity/acceleration, not grind distance. No distance
    ///accumulator is present in the reviewed82DE56B0 update chain.
    float last_grind_distance() const { return 0.0f; }
    static rs::Result<GrindState, std::string> filtered_output(const GrindOutputFields& out);

    // Private in Rust.
    rng::OrientationRandom orientation_random;
    grind_chromosome::Chromosome chromosome;
};

}  // namespace s3::game::physics::grind::runtime
