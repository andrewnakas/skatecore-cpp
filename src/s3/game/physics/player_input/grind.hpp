// Port of skate-game/src/physics/player_input/grind.rs.
//! Host manager82D8A828/82D8AB08, using CURRENT player-input ownership.
//! Static authored-world scope. Gameplay leaves live in skate-core; world
//! filtering, primitive provenance and board material application stay explicit.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/grind_contact/balance.hpp"
#include "s3/core/physics/grind_contact/control.hpp"
#include "s3/core/physics/grind_contact/entry.hpp"
#include "s3/core/physics/grind_contact/manager.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/grind_world.hpp"
#include "s3/game/physics/grind/observation.hpp"
#include "s3/game/physics/player_input/grind/settings.hpp"
#include "s3/game/physics/player_input/grind/world.hpp"

namespace s3::game::physics::player_input::grind {

namespace grind_surface = sc::air::trajectory::grind_surface;
namespace balance = sc::physics::grind_contact::balance;
namespace control = sc::physics::grind_contact::control;
namespace entry = sc::physics::grind_contact::entry;
namespace manager = sc::physics::grind_contact::manager;
using ::s3::game::grind_world::StaticProvider;
using ::s3::game::physics::grind::observation::ManagerObservation;
using grind_surface::GrindSurface;
using grind_surface::Probe;
using grind_surface::ProbeHit;
using sc::physics::board_world::BoardWorld;
using sc::player::input_phase::GrindInvestigationFields;
using sc::player::input_phase::ProcessedPhysicsInput;
using V = std::array<float, 4>;

/// These are real physical/animation producers omitted from the old shared
/// input subset. There are deliberately no guessed defaults.
/// `#[derive(Clone, Copy, Debug)]`
struct PreContext {
    /// Physical Processed64..127, not effective animation transform192.
    std::array<V, 4> board{};
    std::int32_t air_counter = 0;
    std::uint32_t tip_state = 0;
    bool air_targeting_grind_9653 = false;
    float balance_2720 = 0.0f;
    float translation_2796 = 0.0f;
    float stability_nudge_2800 = 0.0f;
    float up_down_2804 = 0.0f;
    float grab_min_height_2808 = 0.0f;
};

///Fresh physical/animation values read by82D8AB08 after intervening producers.
///Candidate selection and submitted geometry hits remain cached in Pending.
/// `#[derive(Clone, Copy, Debug)]`
struct PostContext {
    ///Current physical Processed64..127, not animation transform192.
    std::array<V, 4> board{};
    float balance_2720 = 0.0f;
    float translation_2796 = 0.0f;
    float stability_nudge_2800 = 0.0f;
    float up_down_2804 = 0.0f;
    float grab_min_height_2808 = 0.0f;
};

/// `#[derive(Clone, Copy, Debug, PartialEq, Eq)]`
enum class MaterialMode {
    ///82D89DC0 leaves materials and its air-target history untouched.
    Unchanged,
    ///Restore the initialized standard material triplet (82D89DC0).
    ///Not the broader SetStandard routine's unrelated body/volume writes.
    Standard,
    ///All parts group4; wheels board8316, deck8328, trucks8340.
    Grind,
};

/// `#[derive(Clone, Copy, Debug)]`
struct PrimitiveMetadata {
    std::optional<std::array<std::uint64_t, 2>> spline_guids;
};

/// Callbacks execute the original grind line-batch contract against current
/// world metadata (see world), not the contact-fatness BoardWorld line query.
/// Query errors propagate, never become misses. The host owns actual body and
/// material tables; a material mode must be applied before this call returns.
/// (Rust trait `Host` -> abstract interface.)
struct Host {
    virtual ~Host() = default;
    virtual Status apply_material_mode(MaterialMode mode) = 0;
    virtual rs::Result<std::optional<ProbeHit>, std::string> surface_probe(const BoardWorld& world,
                                                                           std::array<std::uint32_t, 2> actor,
                                                                           std::size_t index, Probe probe) = 0;
    virtual rs::Result<std::optional<balance::ForceExitHit>, std::string> force_exit_line(
        const BoardWorld& world, std::array<std::uint32_t, 2> actor, balance::ForceExitProbe probe) = 0;
};

/// Private in Rust.
struct GeometryWork {
    grind_surface::InvestigationInput input{};
    std::optional<grind_surface::Investigation> plan;
    std::array<std::optional<ProbeHit>, 7> hits{};
};

/// Not Clone: the caller consumes one pre result in one post phase.
struct Pending {
    GrindInvestigationFields fields{};
    std::optional<GeometryWork> geometry;
    std::optional<PrimitiveMetadata> metadata;
};

struct PostResult {
    /// One entry per native Request call, including two simultaneous impacts.
    /// Main delivers these through its actual wipeout manager (which updates
    /// reason flags, timers and request count); processed bit18 is set here.
    std::vector<std::size_t> wipeout_reasons;
    ManagerObservation observation;
};

struct GrindInputState {
    std::uint32_t previous_state = 0;
    std::uint32_t engagement_counter = 0;
    std::uint32_t cooldown = 0;
    bool disabled = false;
    bool suppressed = false;
    float elapsed = 0.0f;
    float friction_vs_time = 0.0f;
    std::array<std::uint32_t, 4> previous_velocity{};
    std::uint32_t grind_history = 0;
    std::uint32_t secondary_history = 0;
    std::uint32_t grounded_frames = 0;
    std::uint32_t air_frames = 0;
    std::uint32_t low_wheel_frames = 0;
    bool previous_proximity = false;
    bool previous_air_target = false;
    V previous_direction{};
    float gravity_timer = 0.0f;
    balance::BalanceState balance{};
    entry::Engagement engagement{};
    control::Control control{};
    /// Physical pop consumes this same cache/energy/cooldown, not a copy.
    manager::Jumper jumper{};
    GrindInvestigationFields investigation{};
    settings::Settings settings{};

    static rs::Result<GrindInputState, std::string> load(const sd::Collections& data);
    ///82D8A638 resets the manager, not the separately constructed child objects.
    void reset();

    // ---- grind/history.rs ----
    ///82D8A828/82D8ABD8; preserved current permission/counter arithmetic.
    void permission(const ProcessedPhysicsInput& p, std::int32_t air_counter);
    ///82D875A8; accepted families RETAIN previous proximity. Only fallback
    ///writes the history byte, which is not the valid-candidate flag.
    void advance_history(const ProcessedPhysicsInput& p);
    ///82D89DC0: use the PRECEDING KnownAir target on ground, then update it.
    MaterialMode material_mode(const ProcessedPhysicsInput& p, bool nearby, bool targeting);

    // ---- grind/pre.rs ----
    ///82D8A828: permission -> bounded static query -> materials -> investigator
    ///-> directed tangent -> previous velocity -> friction -> geometry probes.
    /// Main may run other input producers between this and post_update.
    rs::Result<Pending, std::string> pre_update(const ProcessedPhysicsInput& p, const StaticProvider& provider,
                                                const BoardWorld& world, PreContext context, Host& host);

    // ---- grind/post.rs ----
    ///82D8AB08's meaningful order. Geometry consumes the pre-submitted hits;
    /// material application has already happened. The complete snapshot is
    /// copied only after balance, engagement, controls and jumper publication.
    rs::Result<PostResult, std::string> post_update(ProcessedPhysicsInput& p, const BoardWorld& world,
                                                    Pending pending, PostContext context, Host& host);
};

/// Private helpers.
V float_(std::array<std::uint32_t, 4> v);  // Rust `fn float`
std::array<std::uint32_t, 4> raw(V v);

}  // namespace s3::game::physics::player_input::grind
