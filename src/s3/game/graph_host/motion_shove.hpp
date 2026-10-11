// Port of skate-game/src/graph_host/motion_shove.rs (module `motion_shove`).
//! Original TU3 Shove constructor82BBCCF0, Begin82BBCDD0,
//! Update82BBCDF0 and End82BBD540. The host owns channels and graph values.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_animation {
class MotionAnimation;
}

namespace s3::game::graph_host::motion_shove {

using motion_animation::MotionAnimation;
using s3::data::state_graph_attributes::Attributes;

/// Fields private in Rust.
struct ShoveOperation {
    std::string selection;
    std::string selection_board;
    std::string anticipation;
    std::string anticipation_board;

    static rs::Result<std::optional<ShoveOperation>, std::string> parse(const Attributes& a);
};
S3_DERIVE_EQ(ShoveOperation, selection, selection_board, anticipation, anticipation_board)

struct ShovePhysical {
    ///PhysOutInteraction (bundle52)+56; ProcessOutput82DB7E38 owns this.
    bool interaction_trigger = false;
    ///PhysOutGround240, the interaction direction published by that producer.
    std::array<float, 4> direction{};
    ///PhysOutState75 and PhysOutOffboard311, respectively.
    bool in_biped_category = false;
    bool board_on_ground = false;
    ///PhysOutAnimation72, used only while the shove channel is active.
    float animation_height = 0.0f;
};

struct ShoveState {
    float angle = 0.0f;
    ///Native instance12 records the anticipation selection. Update does not
    ///use this cache to replace the newly observed board state.
    bool board_anticipation = false;

    static ShoveState new_() { return ShoveState{0.0f, false}; }
    void begin() {
        //82BBCDD0 clears both fields; this does not terminate any channel.
        angle = 0.0f;
        board_anticipation = false;
    }
    rs::Result<rs::Unit, std::string> update(const ShoveOperation& operation, MotionAnimation& animation,
                                             ShovePhysical physical, std::array<std::uint32_t, 2> busy_hands,
                                             bool mirrored);
    void end(MotionAnimation& animation, bool keep_channels);
};

///82BBD250..D390: original one-refinement reciprocal followed by82473B98.
///Unlike left-stick angle, zero direction has no extra zero-vector guard. (Private in Rust.)
float direction_angle(std::array<float, 4> direction, bool mirrored);

}  // namespace s3::game::graph_host::motion_shove
