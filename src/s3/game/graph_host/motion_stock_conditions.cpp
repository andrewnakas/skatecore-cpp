// Port of skate-game/src/graph_host/motion_stock_conditions.rs (out-of-line items).
#include "s3/game/graph_host/motion_stock_conditions.hpp"

#include "s3/game/graph_host/condition_nodes.hpp"

namespace s3::game::graph_host::motion_stock_conditions {

bool Condition::recognizes(std::string_view name) {
    return name == "CanLandOnBoard" || name == "DistToEdge" || name == "IsDeckFree" ||
           name == "IsBipedCommittedToMotion" || name == "EnoughDistToObstacle" || name == "CanBipedLand" ||
           name == "IsCrouchedEnoughForBlendToGrabCycle" || name == "TrucksOrDeckInContact" ||
           name == "PhysicsWantsManualExit" || name == "ApexReached";
}

Condition Condition::parse(const Attributes& a) {
    const std::string* name_ptr = a.text("name");
    const std::string name = name_ptr ? *name_ptr : std::string();
    if (name == "CanLandOnBoard") return CanLandOnBoard{};
    if (name == "DistToEdge") return DistToEdge{condition_nodes::numeric(a)};
    if (name == "IsDeckFree") return IsDeckFree{};
    if (name == "IsBipedCommittedToMotion") return IsBipedCommittedToMotion{};
    if (name == "EnoughDistToObstacle") {
        const std::string* database = a.text("db");
        const std::string* animation = a.text("anim");
        return EnoughDistToObstacle{database ? *database : std::string(), animation ? *animation : std::string()};
    }
    if (name == "CanBipedLand") return CanBipedLand{};
    if (name == "IsCrouchedEnoughForBlendToGrabCycle") return IsCrouchedEnoughForBlendToGrabCycle{};
    if (name == "TrucksOrDeckInContact") return TrucksOrDeckInContact{};
    if (name == "PhysicsWantsManualExit") return PhysicsWantsManualExit{};
    if (name == "ApexReached") return ApexReached{};
    S3_PANIC("internal error: entered unreachable code");
}

}  // namespace s3::game::graph_host::motion_stock_conditions
