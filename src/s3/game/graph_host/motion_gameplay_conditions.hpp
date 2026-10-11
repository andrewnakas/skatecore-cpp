// Port of skate-game/src/graph_host/motion_gameplay_conditions.rs.
//! Original TU3 physical-state condition leaves used by the stock MotionGraph.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_gameplay_conditions {

using s3::data::state_graph_attributes::Attributes;

/// The preceding completed physical publication. These are native output
/// fields, separate from controller intents and presentation transforms.
struct GameplayConditions {
    std::uint32_t state = 0;
    bool wants_runout = false;
    bool physics_wiping = false;
    bool body_flipping = false;
    bool wants_wipeout = false;
    bool bumped = false;
    bool grabbing_object = false;
    bool retrieving_board = false;
    bool dropping_board = false;
    bool in_biped_air = false;
    bool hippy_hurdling = false;
    std::uint32_t handplant_flags = 0;
    float handplant_time = 0.0f;
    /// anim_handplant/default layout0/4/8: out, into, antic.
    std::array<float, 3> handplant_thresholds{};
    bool footplant_active = false;
    float footplant_duration = 0.0f;
    float footplant_contact_time = 0.0f;
    float time_to_skitch = 0.0f;
    float skitch_transition_time = 0.0f;
    /// TimeToLand82BA7250: PhysOutAir+184, gated by byte437.
    float time_to_land = 0.0f;
    bool time_to_land_valid = false;
    /// OBTimeToLand82BA5770: PhysOutOffBoard+32.
    float offboard_time_to_land = 0.0f;
    /// OffboardBodyTweakBlend82BBAFC8 reads completed OffBoard+92.
    float offboard_air_scalar_92 = 0.0f;
    /// MatchAirTime82BBA200 consumes the completed OffBoard+96 vector.
    std::array<float, 4> offboard_air_translation{};
    /// CanBipedLand82BA8214 tests OffBoard+192.Y strictly above0.85.
    std::array<float, 4> offboard_landing_normal{};
    ///IsBipedCommittedToMotion82BA80A0: completed OffBoard byte329.
    bool offboard_committed_to_motion = false;
    ///EnoughDistToObstacle82BA7FD0: completed OffBoard+112, not distance116.
    float offboard_obstacle_distance = 0.0f;
    ///DistToEdge82BA82A8: completed OffBoard+116, not obstacle scalar112.
    float offboard_edge_distance = 0.0f;
    /// OBTrajTime82BA4528: PhysOutOffBoard+120, gated by byte331.
    float offboard_trajectory_time = 0.0f;
    bool offboard_trajectory_valid = false;
    /// AirOutputFields::reached_apex_436, published by the known-air owner.
    bool reached_apex = false;
    ///TU3 CanLandOnBoard82BA5D30 and LandOnBoard82BB95D8.
    bool can_land_on_board = false;
    bool landing_turning = false;
    /// GrindOutputFields::flag_318/322, published by the contact solver.
    bool grind_contact = false;
    /// CollisionOutputFields::wheel_contact_3296_3299.
    bool wheel_contact = false;
    /// Collision3472 (trucks) OR3475 (deck), excluding wheels.
    bool trucks_or_deck_contact = false;
    /// StateGraph's moving-object state publication. TU3 camera and motion
    /// consumers use state 502 for the moving-object riding branch.
    bool moving_object = false;
    bool tricks_blocked_on_stairs = false;
};

/// Rust `enum GameplayCondition`; each variant is a nested struct held in `value`.
struct GameplayCondition {
    struct RetrievingBoard {};
    struct DroppingBoard {};
    struct InBipedAir {};
    struct HippyHurdling {};
    struct WantsRunout {};
    struct PhysicsWiping {};
    struct BodyFlipping {};
    struct WantsWipeout {};
    struct Bumped {};
    struct CanEnterSlide {
        bool right = false;
    };
    struct GrabbingObject {};
    struct Dark {};
    struct UnderflipRequested {};
    struct DarkCatchRequested {};
    struct OkToDoTrickOnStairs {};
    struct HandPlanting {
        std::uint8_t state = 0;
        std::uint8_t direction = 0;
    };
    struct FootPlanting {};
    struct PrepareFootplant {};
    struct NewHandplantPosition {};
    struct PlayHandplant {
        std::size_t phase = 0;
    };
    struct EnteringSkitch {};
    struct Skitching {};
    struct IsMovingObject {};

    using Value = std::variant<RetrievingBoard, DroppingBoard, InBipedAir, HippyHurdling, WantsRunout, PhysicsWiping,
                               BodyFlipping, WantsWipeout, Bumped, CanEnterSlide, GrabbingObject, Dark,
                               UnderflipRequested, DarkCatchRequested, OkToDoTrickOnStairs, HandPlanting,
                               FootPlanting, PrepareFootplant, NewHandplantPosition, PlayHandplant, EnteringSkitch,
                               Skitching, IsMovingObject>;
    Value value;

    template <class V, class = std::enable_if_t<std::is_constructible_v<Value, V>>>
    GameplayCondition(V v) : value(std::move(v)) {}

    template <class V>
    bool is() const {
        return std::holds_alternative<V>(value);
    }

    static bool recognizes(std::string_view name);
    static rs::Result<GameplayCondition, std::string> parse(const Attributes& a);
    /// Both stock graphs register these same native leaves. Return None for
    /// conditions that also require the MotionGraph/channel owner.
    std::optional<bool> evaluate_physical(const GameplayConditions& p) const;

    /// `evaluate(&self, host: &MotionHost)`. Generic over the MotionGraph host (graph_host::motion,
    /// ported separately): it reads `gameplay_conditions`, `animation.channels.has`,
    /// `animation.skater_animation_flags`, `riding.dark`, `trick_requests` and `slide_latch.start`.
    template <class MotionHost>
    rs::Result<bool, std::string> evaluate(const MotionHost& host) const {
        if (!host.gameplay_conditions) {
            return rs::make_err(std::string("MotionGraph requires the actual physical condition publication"));
        }
        const GameplayConditions& p = *host.gameplay_conditions;
        if (const std::optional<bool> result = evaluate_physical(p)) return *result;
        //82BA5F60: Offboard322 or the actual RetrieveBoard channel.
        if (is<DroppingBoard>()) return p.dropping_board || host.animation.channels.has("RetrieveBoard");
        //82BA79A0 calls the specific MotionGraph getter8258FB68.
        if (is<Dark>()) return static_cast<bool>(host.riding.dark);
        if (is<UnderflipRequested>()) return static_cast<bool>(host.trick_requests.underflip);
        if (is<DarkCatchRequested>()) return static_cast<bool>(host.trick_requests.dark_catch);
        if (const auto* c = std::get_if<CanEnterSlide>(&value)) {
            //82BA6FE0 rejects RevertGround102, reverses the side for fakie,
            //then reads its start bit in the retained C84 slide packet.
            if (p.state == 102) return false;
            //ISkaterAnim virtual12=82B970D8 reads bit29=fakie.
            if (!host.animation.skater_animation_flags) {
                return rs::make_err(std::string("CanEnterSlide requires actual SkaterAnim stance flags"));
            }
            const bool fakie = (*host.animation.skater_animation_flags & 0x20000000u) != 0;
            if (c->right ^ fakie) return static_cast<bool>(host.slide_latch.start(true));
            return static_cast<bool>(host.slide_latch.start(false));
        }
        S3_PANIC("internal error: entered unreachable code: Physical condition handled above");
    }
};
S3_DERIVE_EQ_EMPTY(GameplayCondition::RetrievingBoard)
S3_DERIVE_EQ_EMPTY(GameplayCondition::DroppingBoard)
S3_DERIVE_EQ_EMPTY(GameplayCondition::InBipedAir)
S3_DERIVE_EQ_EMPTY(GameplayCondition::HippyHurdling)
S3_DERIVE_EQ_EMPTY(GameplayCondition::WantsRunout)
S3_DERIVE_EQ_EMPTY(GameplayCondition::PhysicsWiping)
S3_DERIVE_EQ_EMPTY(GameplayCondition::BodyFlipping)
S3_DERIVE_EQ_EMPTY(GameplayCondition::WantsWipeout)
S3_DERIVE_EQ_EMPTY(GameplayCondition::Bumped)
S3_DERIVE_EQ(GameplayCondition::CanEnterSlide, right)
S3_DERIVE_EQ_EMPTY(GameplayCondition::GrabbingObject)
S3_DERIVE_EQ_EMPTY(GameplayCondition::Dark)
S3_DERIVE_EQ_EMPTY(GameplayCondition::UnderflipRequested)
S3_DERIVE_EQ_EMPTY(GameplayCondition::DarkCatchRequested)
S3_DERIVE_EQ_EMPTY(GameplayCondition::OkToDoTrickOnStairs)
S3_DERIVE_EQ(GameplayCondition::HandPlanting, state, direction)
S3_DERIVE_EQ_EMPTY(GameplayCondition::FootPlanting)
S3_DERIVE_EQ_EMPTY(GameplayCondition::PrepareFootplant)
S3_DERIVE_EQ_EMPTY(GameplayCondition::NewHandplantPosition)
S3_DERIVE_EQ(GameplayCondition::PlayHandplant, phase)
S3_DERIVE_EQ_EMPTY(GameplayCondition::EnteringSkitch)
S3_DERIVE_EQ_EMPTY(GameplayCondition::Skitching)
S3_DERIVE_EQ_EMPTY(GameplayCondition::IsMovingObject)
S3_DERIVE_EQ(GameplayCondition, value)

}  // namespace s3::game::graph_host::motion_gameplay_conditions
