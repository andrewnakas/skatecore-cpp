// Port of skate-game/src/graph_host/motion_hand_services.rs (module `motion_hand_services`).
#include "s3/game/graph_host/motion_hand_services.hpp"

#include <string>

#include "s3/game/graph_host/motion_animation.hpp"

namespace s3::game::graph_host::motion_hand_services {

std::optional<Operation> Operation::parse(const Attributes& attributes) {
    const std::string* name = attributes.text("name");
    if (name == nullptr) return std::nullopt;
    if (*name == "HandBusy") {
        const std::string* hand = attributes.text("hand");
        return Operation(HandBusy{hand_from_name(hand ? std::string_view(*hand) : std::string_view("bs"))});
    }
    if (*name == "MaintainShove") return Operation(MaintainShove{});
    return std::nullopt;
}

void HandServices::execute(Operation operation, std::uint8_t phase, MotionAnimation& animation) {
    if (apply(operation, phase) && animation.channels.has("SkitchAntic")) {
        // End82BBD6E8 first clears the retention flag, then requests
        // premature channel end (virtual44), preserving its fade timing.
        animation.channels.end("SkitchAntic");
    }
}

bool HandServices::apply(Operation operation, std::uint8_t phase) {
    if (const auto* busy = std::get_if<Operation::HandBusy>(&operation.value)) {
        std::uint32_t& count = busy_hands[hand_index(busy->_0)];
        if (phase == 0) {
            count = count + 1u;
        } else if (phase == 1) {
            // HandBusy Update vtable8231F97C+52 is blr82B61BB8.
        } else {
            count = count - 1u;
        }
        return false;
    }
    if (phase == 0) {
        // Begin vtable82321000+48 is blr82B61BB8.
    } else if (phase == 1) {
        keep_shove_channels = true;
    } else {
        keep_shove_channels = false;
        return true;
    }
    return false;
}

}  // namespace s3::game::graph_host::motion_hand_services
