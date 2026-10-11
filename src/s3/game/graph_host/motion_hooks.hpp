// Port of skate-game/src/graph_host/motion_hooks.rs.
//! Native MotionGraph transition hooks, dispatched by the stock controller.
#pragma once
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "s3/core/animation/playback.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_hooks {

using s3::animation::playback::TransitionSettings;
using s3::data::state_graph_attributes::Attributes;

/// Rust `enum MotionHook`; each variant is a nested struct held in `value`.
struct MotionHook {
    ///82BBBB68 copies the authored transition into the next-play override.
    struct Override {
        TransitionSettings _0;
    };
    struct MongoPushToAntic {
        std::string animation;
    };
    ///82BBB848, factory82BCA6A8 default right=true.
    struct GrabSlide {
        bool right = false;
    };

    using Value = std::variant<Override, MongoPushToAntic, GrabSlide>;
    Value value;

    template <class V, class = std::enable_if_t<std::is_constructible_v<Value, V>>>
    MotionHook(V v) : value(std::move(v)) {}

    template <class V>
    bool is() const {
        return std::holds_alternative<V>(value);
    }

    static std::optional<MotionHook> parse(const Attributes& a);
};
S3_DERIVE_EQ(MotionHook::Override, _0)
S3_DERIVE_EQ(MotionHook::MongoPushToAntic, animation)
S3_DERIVE_EQ(MotionHook::GrabSlide, right)
S3_DERIVE_EQ(MotionHook, value)

}  // namespace s3::game::graph_host::motion_hooks
