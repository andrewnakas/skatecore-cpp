// Port of skate-game/src/physics/grind_chromosome.rs.
//! ZIP chromosome port corrected from S3 82DEE918/82DEF518/82DEE508.
//! S2 82E2E9E0/82E2EBE8/82E2F9D0 confirms history/publication structure;
//! only S3 has four-way orientation for5050/5O and the sixth darkslide family.
#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <optional>

#include "s3/core/player/input_phase/grind_output.hpp"
#include "s3/core/rs.hpp"
#include "s3/game/physics/grind/family.hpp"
#include "s3/game/physics/grind_chromosome/pose.hpp"
#include "s3/game/physics/grind_names.hpp"

namespace s3::game::physics::grind_chromosome {

using grind::Family;
/// `#[path = "grind_names.rs"] pub(crate) mod names;`
namespace names = ::s3::game::physics::grind_names;
using pose::ApproachPose;
using pose::Input;

struct Components {
    std::array<std::uint32_t, 6> _0{};

    names::Name name() const;
};
S3_DERIVE_EQ(Components, _0)

struct Publication {
    Components volatile_;
    std::optional<Components> animation;
    std::optional<Components> scoring;
};
S3_DERIVE_EQ(Publication, volatile_, animation, scoring)

///82DEE508's actual PhysOut publication, after update and only while grinding.
/// FastString encoding uses CURRENT's existing original-name encoder.
void publish(const Publication& p, ::s3::player::input_phase::grind_output::GrindOutputFields& out);

struct Chromosome {
    ///82DEE358 leaves byte144 unwritten. The false storage below is opaque
    ///until the host observes the first completed animation packet.
    static Chromosome uninitialized();
    ///Explicit host initialization of a native unwritten byte, not a claim
    ///about retail allocator contents. Seed once from completed Anim10372;
    ///do not replace constructor axes/position or any established history.
    void initialize_host_fakie(bool observed_fakie);
    /// S3 ctor82DEE358 explicitly initializes the saved axes/position but does
    /// not write saved fakie byte144. The supplied value is an explicit host
    /// initialization observation, not a recovered native constructor default.
    /// A real Air439 snapshot or30 Ground samples replaces this reference.
    static Chromosome new_(bool constructor_fakie_144);
    /// Invoke after physical selection/FillOut with current PhysOut inputs.
    /// Category400 and grinding316 are distinct source observations.
    std::optional<Publication> update(const Input& input);

    // Private in Rust.
    ApproachPose reference() const;
    std::uint32_t travel(const Input& input, Family family);

    std::deque<ApproachPose> history;
    ApproachPose saved;
    bool saved_fakie_initialized = false;
    std::uint32_t approach = 0;
    std::uint32_t previous_category = 0;
    std::optional<Family> previous_kind;
    std::int32_t away_frames = 0;
    bool reversed = false;
    std::optional<std::uint32_t> orientation;
    std::optional<Components> pending;
    std::int32_t pending_frames = 0;
    std::optional<Components> animation;
    std::optional<Components> scoring;
};

}  // namespace s3::game::physics::grind_chromosome
