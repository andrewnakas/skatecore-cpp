// Port of skate-game/src/graph_host/motion_hand_services.rs (module `motion_hand_services`).
//! TU3 HandBusy and MaintainShove lifecycle, independently recovered from IDA.
//! HandBusy: factory82BC8B08, Begin82BAB4D0, End82BAB588.
//! MaintainShove: factory82BCB1C8, Update82BBD690, End82BBD6E8.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_animation {
class MotionAnimation;
}

namespace s3::game::graph_host::motion_hand_services {

using motion_animation::MotionAnimation;
using s3::data::state_graph_attributes::Attributes;

enum class Hand {
    Backside,
    Frontside,
};

/// `Hand::from_name` (private in Rust).
inline Hand hand_from_name(std::string_view name) {
    // Factory compares against bs with strcmp82AE89B0. Every unequal
    // spelling selects index1; there is no validation or stance swap.
    return name == "bs" ? Hand::Backside : Hand::Frontside;
}
/// `Hand::index` (private in Rust).
inline std::size_t hand_index(Hand self) { return self == Hand::Backside ? 0 : 1; }

/// Rust `enum Operation { HandBusy(Hand), MaintainShove }`.
struct Operation {
    struct HandBusy {
        Hand _0 = Hand::Backside;
    };
    struct MaintainShove {};

    std::variant<HandBusy, MaintainShove> value;

    Operation(HandBusy v) : value(v) {}
    Operation(MaintainShove v) : value(v) {}

    static std::optional<Operation> parse(const Attributes& attributes);
};
S3_DERIVE_EQ(Operation::HandBusy, _0)
S3_DERIVE_EQ_EMPTY(Operation::MaintainShove)
S3_DERIVE_EQ(Operation, value)

/// One persistent owner shared by all graph instances and hand consumers.
struct HandServices {
    std::array<std::uint32_t, 2> busy_hands{};
    bool keep_shove_channels = false;

    void execute(Operation operation, std::uint8_t phase, MotionAnimation& animation);
    /// Private in Rust (its tests call it).
    bool apply(Operation operation, std::uint8_t phase);
};

}  // namespace s3::game::graph_host::motion_hand_services
