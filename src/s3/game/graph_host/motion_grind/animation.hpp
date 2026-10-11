// Port of skate-game/src/graph_host/motion_grind/animation.rs (module `motion_grind::animation`).
//! Begin82BB0A30: live set/apply/query, including partial output on a miss.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/rs.hpp"

namespace s3::game::graph_host::motion_grind::animation {

using s3::animation::output::attributes::AnimationAttribute;
using s3::animation::output::attributes::AttributeName;
using s3::animation::output::attributes::MotionGraphAttribute;
using s3::animation::playback_parameters::SettableAttribute;

/// Rust `trait Animation: AttributeSink`: existing animation services plus a per-name query of
/// its CURRENT root. No clone/probe tree, destination override, attribute enumeration or clock
/// step. Ported as a compile-time concept (callers are generic, as in Rust's `impl Animation`):
///   void set_attribute(SettableAttribute);
///   rs::Result<rs::Unit, std::string> apply_parameters();
///   rs::Result<bool, std::string> query_current_attribute(AttributeName name, std::uint32_t mask,
///                                                         AnimationAttribute& output) const;
///   void emit_packet(AttributeName name, float value);
///   std::optional<float> intent(std::string_view name) const;

/// `pub(super) fn set`.
template <class Sink>
void set(Sink& animation, AttributeName name, float value, bool normalized) {
    animation.set_attribute(SettableAttribute{name, value, normalized, -1});
}

template <class A>
rs::Result<std::array<float, 2>, std::string> endpoints(A& animation, AttributeName name) {
    std::array<float, 2> bounds{0.0f, 0.0f};
    for (std::size_t i = 0; i < bounds.size(); ++i) {
        set(animation, name, static_cast<float>(i), true);
        auto applied = animation.apply_parameters();
        if (applied.is_err()) return rs::make_err(applied.unwrap_err());
        AnimationAttribute attribute = MotionGraphAttribute{name, 0.0f}.to_animation();
        // Native ignores the success boolean. Preserve a child's partial write;
        // do not reset to zero on false, nor conflate data errors with a miss.
        auto queried = animation.query_current_attribute(name, 15, attribute);
        if (queried.is_err()) return rs::make_err(queried.unwrap_err());
        if (!attribute.payload._0[0]) {
            return rs::make_err(std::string("Grind twist query returned an uninitialized scalar"));
        }
        bounds[i] = rs::from_bits(*attribute.payload._0[0]);
    }
    return bounds;
}

}  // namespace s3::game::graph_host::motion_grind::animation
