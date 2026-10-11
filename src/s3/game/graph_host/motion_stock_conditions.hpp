// Port of skate-game/src/graph_host/motion_stock_conditions.rs.
//! Recovered stock predicates using completed physical publications.
//! Unsupported archive guesses are deliberately left to the graph's diagnostic path.
#pragma once
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "s3/core/graph/conditions.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_stock_conditions {

using s3::data::state_graph_attributes::Attributes;
using s3::graph::conditions::NumericCondition;

/// (Private in Rust; exposed for the ported tests.)
inline bool enough_distance(float distance, float translation) {
    //82BA7FF0 fadds;7FF4 fcmpu;7FF8 blt. Preserve unordered behavior.
    return !(distance < translation + rs::from_bits(0x3e99999au));
}

/// Rust `enum Condition`; each variant is a nested struct held in `value`.
struct Condition {
    struct CanLandOnBoard {};
    struct DistToEdge {
        NumericCondition _0;
    };
    struct IsDeckFree {};
    struct IsBipedCommittedToMotion {};
    struct EnoughDistToObstacle {
        std::string database;
        std::string animation;
    };
    struct CanBipedLand {};
    struct IsCrouchedEnoughForBlendToGrabCycle {};
    struct TrucksOrDeckInContact {};
    struct PhysicsWantsManualExit {};
    struct ApexReached {};

    using Value = std::variant<CanLandOnBoard, DistToEdge, IsDeckFree, IsBipedCommittedToMotion, EnoughDistToObstacle,
                               CanBipedLand, IsCrouchedEnoughForBlendToGrabCycle, TrucksOrDeckInContact,
                               PhysicsWantsManualExit, ApexReached>;
    Value value;

    template <class V, class = std::enable_if_t<std::is_constructible_v<Value, V>>>
    Condition(V v) : value(std::move(v)) {}

    template <class V>
    bool is() const {
        return std::holds_alternative<V>(value);
    }

    static bool recognizes(std::string_view name);
    /// Panics (Rust `unreachable!`) on a name `recognizes` rejects.
    static Condition parse(const Attributes& a);

    /// `evaluate(&self, host: &MotionHost)`. Generic over the MotionGraph host (graph_host::motion,
    /// ported separately): it reads `gameplay_conditions`, `toggle_board_physical`,
    /// `animation.stock_clip_translation_z`, `crouching_physical` and `manual_exit`.
    template <class MotionHost>
    rs::Result<bool, std::string> evaluate(const MotionHost& host) const {
        if (!host.gameplay_conditions) {
            return rs::make_err(std::string("stock gameplay condition requires physical publication"));
        }
        const auto& p = *host.gameplay_conditions;
        if (is<CanLandOnBoard>()) return static_cast<bool>(p.can_land_on_board);
        //82BA8180: no Air437/time-valid or moving-object gate.
        if (is<CanBipedLand>()) return p.offboard_landing_normal[1] > 0.85f;
        //82BA5ED8 reads byte312 independently of held311.
        if (is<IsDeckFree>()) {
            if (!host.toggle_board_physical) return rs::make_err(std::string("IsDeckFree requires completed OffBoard312"));
            return static_cast<bool>(host.toggle_board_physical->free_board);
        }
        //82BA80FC reads completed OffBoard329.
        if (is<IsBipedCommittedToMotion>()) return static_cast<bool>(p.offboard_committed_to_motion);
        //82BA42E0 reads Collision3472 OR3475, excluding wheel contacts.
        if (is<TrucksOrDeckInContact>()) return static_cast<bool>(p.trucks_or_deck_contact);
        if (is<ApexReached>()) return static_cast<bool>(p.reached_apex);
        //82BA8238 compares completed OffBoard116.
        if (const auto* c = std::get_if<DistToEdge>(&value)) return c->_0.matches(p.offboard_edge_distance);
        if (const auto* c = std::get_if<EnoughDistToObstacle>(&value)) {
            const rs::Result<float, std::string> translation =
                host.animation.stock_clip_translation_z(c->database, c->animation);
            if (translation.is_err()) return rs::make_err(translation.unwrap_err());
            return enough_distance(p.offboard_obstacle_distance, translation.unwrap());
        }
        //82BBDE40 compares PhysOutAnimation72 with literal821EE79C.
        if (is<IsCrouchedEnoughForBlendToGrabCycle>()) {
            if (!host.crouching_physical) {
                return rs::make_err(std::string("Crouch condition requires physical animation height"));
            }
            return host.crouching_physical->animation_height_72 < rs::from_bits(0x3f19999au);
        }
        //82BA7930 reads completed Animation168. (PhysicsWantsManualExit)
        if (!host.manual_exit) {
            return rs::make_err(std::string("PhysicsWantsManualExit requires completed physical animation output"));
        }
        return static_cast<bool>(*host.manual_exit);
    }
};
S3_DERIVE_EQ_EMPTY(Condition::CanLandOnBoard)
S3_DERIVE_EQ(Condition::DistToEdge, _0)
S3_DERIVE_EQ_EMPTY(Condition::IsDeckFree)
S3_DERIVE_EQ_EMPTY(Condition::IsBipedCommittedToMotion)
S3_DERIVE_EQ(Condition::EnoughDistToObstacle, database, animation)
S3_DERIVE_EQ_EMPTY(Condition::CanBipedLand)
S3_DERIVE_EQ_EMPTY(Condition::IsCrouchedEnoughForBlendToGrabCycle)
S3_DERIVE_EQ_EMPTY(Condition::TrucksOrDeckInContact)
S3_DERIVE_EQ_EMPTY(Condition::PhysicsWantsManualExit)
S3_DERIVE_EQ_EMPTY(Condition::ApexReached)
S3_DERIVE_EQ(Condition, value)

}  // namespace s3::game::graph_host::motion_stock_conditions
