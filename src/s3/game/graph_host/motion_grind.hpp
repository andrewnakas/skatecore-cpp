// Port of skate-game/src/graph_host/motion_grind.rs (module `motion_grind`).
//! Original S3 grind graph lifecycle, using the current animation owner.
//! No animation choice, physical-state transition or synthetic observation.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/animation/grind_control.hpp"
#include "s3/core/animation/grind_control/facing.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/graph_host/motion_grind/animation.hpp"
#include "s3/game/graph_host/motion_grind/conditions.hpp"
#include "s3/game/graph_host/motion_grind/settings.hpp"

namespace s3::game::graph_host::motion_grind {

using settings::Settings;  // pub use settings::Settings;
using s3::animation::output::attributes::AttributeName;

/// Rust `enum Operation`; each variant is a nested struct held in `value`.
struct Operation {
    struct Attributes {};
    struct Crouch {
        AttributeName height;
    };
    struct Fade {
        AttributeName height;
        AttributeName twist;
        std::string intent;
    };

    std::variant<Attributes, Crouch, Fade> value;

    Operation(Attributes v) : value(v) {}
    Operation(Crouch v) : value(v) {}
    Operation(Fade v) : value(std::move(v)) {}

    static rs::Result<std::optional<Operation>, std::string> parse(
        const s3::data::state_graph_attributes::Attributes& a);
};
S3_DERIVE_EQ_EMPTY(Operation::Attributes)
S3_DERIVE_EQ(Operation::Crouch, height)
S3_DERIVE_EQ(Operation::Fade, height, twist, intent)
S3_DERIVE_EQ(Operation, value)

/// Per behavior instance, never shared between different graph nodes.
struct State {
    std::optional<s3::animation::grind_control::Fade> fade;
    std::optional<float> height;
};

/// Required genuine physical fields. No Default: an unavailable producer must
/// not silently become a fabricated all-zero observation.
struct Physical {
    /// Filtered bundle64 byte80 and five-word encoded name at+8.
    bool grinding;
    AttributeName grind_name;
    /// Motion bundle4 vector80 (deck velocity), Basic bundle0 vector128
    /// (completed effective board forward). Historical field name retained.
    std::array<float, 3> ground_axis;
    std::array<float, 3> board_axis;
    /// Motion273: Processed2468 bit20, not a ground-contact flag.
    bool ground_flag_273;
    /// Live animation mirror, not a cached physical stance.
    bool animation_mirrored;
    /// Animation bundle56 float72, Filtered bundle64 float56.
    float height;
    float crouch;
    /// RAW Skeleton bundle20 float504; this handler mirrors it on Begin.
    float twist;
};

/// phase0=Begin,1=Update,2=End. Graph execution owns lifecycle/order.
/// `A` satisfies the `motion_grind::animation::Animation` concept.
template <class A>
rs::Result<rs::Unit, std::string> execute(State& state, const Operation& op, std::uint8_t phase, float dt,
                                          const Settings& settings, const Physical& physical, A& animation) {
    using s3::animation::skeleton_input::name::encode;
    namespace grind_control = s3::animation::grind_control;
    if (std::holds_alternative<Operation::Attributes>(op.value)) {
        if (phase == 1 && physical.grinding) {
            // Actor1800 slot16 ->8258F6E0 AddMgAttribute, NOT InsertIntent.
            animation.emit_packet(physical.grind_name, 1.0f);
            const bool backwards =
                grind_control::facing::backwards(physical.ground_axis, physical.board_axis,
                                                 physical.ground_flag_273, physical.animation_mirrored);
            animation.emit_packet(encode(backwards ? "GrindFacingBackwards" : "GrindFacingForwards"), 1.0f);
        }
    } else if (const auto* crouch = std::get_if<Operation::Crouch>(&op.value)) {
        if (phase == 0) {
            state.height = physical.height;
        } else if (phase == 1) {
            if (!state.height) return rs::make_err(std::string("ControlGrindCrouch Update before Begin"));
            const float previous = *state.height;
            const auto [min, max, rate] = settings.height;
            const std::optional<float> intent = animation.intent("Crouch");
            const float next = grind_control::crouch(previous, intent.value_or(0.0f), physical.crouch, min, max,
                                                     rate, dt);
            state.height = next;
            animation::set(animation, crouch->height, next, false);
        }
    } else if (const auto* fade_op = std::get_if<Operation::Fade>(&op.value)) {
        if (phase == 0) {
            animation::set(animation, fade_op->height, physical.height, false);
            auto bounds = animation::endpoints(animation, fade_op->twist);
            if (bounds.is_err()) return rs::make_err(bounds.unwrap_err());
            grind_control::Fade fade{};
            const float value = fade.begin(bounds.unwrap()[0], bounds.unwrap()[1],
                                           grind_control::mirrored_twist(physical.twist, physical.animation_mirrored));
            animation::set(animation, fade_op->twist, value, false);
            state.fade = fade;
        } else if (phase == 1) {
            if (!state.fade) return rs::make_err(std::string("GrindControlFade Update before Begin"));
            const std::optional<float> intent = animation.intent(fade_op->intent);
            if (const std::optional<float> value = state.fade->update(dt, intent.value_or(0.0f), settings.fade)) {
                animation::set(animation, fade_op->twist, *value, false);
            }
        }
    }
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion_grind
