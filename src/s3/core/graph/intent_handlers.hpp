// Port of skate-core/src/graph/intent_handlers.rs.
//! Source-backed constant and clock-derived MotionGraph intent behaviours.
//!
//! These are the small stateful operation boundaries used by the stock graph
//! host. They deliberately return the existing `IntentMutation` protocol so
//! the host remains the sole owner of the intent map.
#pragma once
#include <optional>

#include "s3/core/animation/compat/input_graph_intents.hpp"
#include "s3/core/rs.hpp"

namespace s3::graph::intent_handlers {

using input::graph_intents::IntentMutation;

/// TU3 `CreateConstMGIntent` (factory 0x82BC26D8, ctor 0x82BA1768,
/// Begin 0x82BA18A0, Update 0x82BA1938, End 0x82BA1988/0x82BA1990).
struct ConstMgIntent {
    float value = 0.0f;
    bool on_update = false;
    bool created = false;

    static constexpr ConstMgIntent new_(float value, bool on_update) {
        return ConstMgIntent{value, on_update, false};
    }

    /// Begin publishes the authored constant and marks the instance alive.
    IntentMutation begin() {
        created = true;
        return IntentMutation::Set{value};
    }

    /// TU3 does not republish the constant when `onUpdate` is true. It keeps
    /// the existing map value. A false `onUpdate` instance removes its value
    /// only after the begin frame has elapsed.
    IntentMutation update() {
        IntentMutation mutation = (on_update || created) ? IntentMutation(IntentMutation::None{})
                                                         : IntentMutation(IntentMutation::Remove{});
        created = false;
        return mutation;
    }

    /// End removes the owned intent unconditionally. Native End only emits
    /// the remove operation; instance storage is released by the host.
    IntentMutation end() const { return IntentMutation::Remove{}; }
};
S3_DERIVE_EQ(ConstMgIntent, value, on_update, created)

/// TU3 `CreateMGTimeIntentFromAGIntent` (factory 0x82BC2778, ctor 0x82BA3040,
/// Update 0x82BA3158, End 0x82BA28B8). The graph-clock value is supplied by
/// the host; this operation only applies the verified dt accumulation rule.
struct TimeMgIntent {
    float elapsed = 0.0f;

    /// The AG lookup is an existence check only. Any present value advances
    /// the instance clock by the caller's dt; its amplitude is not consumed.
    /// An absent AG key resets the clock and removes the MG entry.
    IntentMutation update(std::optional<float> action_value, float dt) {
        if (action_value) {
            elapsed += dt;
            return IntentMutation::Set{elapsed};
        }
        elapsed = 0.0f;
        return IntentMutation::Remove{};
    }

    /// Native End removes the map entry and does not reset elapsed state.
    IntentMutation end() const { return IntentMutation::Remove{}; }
};
S3_DERIVE_EQ(TimeMgIntent, elapsed)

}  // namespace s3::graph::intent_handlers
