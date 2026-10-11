// Port of skate-game/src/graph_host/pushing.rs (out-of-line items).
#include "s3/game/graph_host/pushing.hpp"

namespace s3::game::graph_host::pushing {

std::optional<PushOperation> PushOperation::from_attributes(const Attributes& attributes) {
    const std::string* name = attributes.text("name");
    if (name == nullptr) return std::nullopt;
    if (*name == "InitPush") return PushOperation(InitPush{});
    if (*name == "ComputeFirstPushStrength") return PushOperation(ComputeFirstPushStrength{});
    if (*name == "ComputeTargetCoefsFromSpeedAndStrength")
        return PushOperation(
            ComputeTargetCoefsFromSpeedAndStrength{attributes.boolean_byte("regularAttributes", 1) != 0});
    if (*name == "ComputeRepushDeadline")
        return PushOperation(ComputeRepushDeadline{attributes.boolean_byte("regularAttributes", 1) != 0});
    if (*name == "SetPushCoefs")
        return PushOperation(SetPushCoefs{attributes.boolean_byte("onFirstUpdateOnly", 0) != 0});
    if (*name == "PushCycle") {
        const std::string* new_push_name = attributes.text("newPushName");
        return PushOperation(PushCycle{new_push_name ? *new_push_name : std::string("LeftPush")});
    }
    if (*name == "PushOut") return PushOperation(PushOut{attributes.boolean_byte("isRightFoot", 1) != 0});
    return std::nullopt;
}

}  // namespace s3::game::graph_host::pushing
