// Port of skate-game/src/graph_host/motion_filter_execute.rs (module `motion::filter_execution`).
#include "s3/game/graph_host/motion/filter_execution.hpp"

#include "s3/game/graph_host/motion/execution.hpp"

namespace s3::game::graph_host::motion {

rs::Result<rs::Unit, std::string> MotionHost::execute_intent_filter(BehaviorId behavior,
                                                                    motion_intent_filter::Operation operation,
                                                                    const Frame& frame, std::uint8_t phase) {
    if (behavior >= instances.size()) return rs::make_err(std::string("Unallocated intent-filter behavior"));
    auto* state = instances[behavior].get<Instance::IntentFilter>();
    if (state == nullptr) return rs::make_err(std::string("Intent-filter operation/instance mismatch"));
    switch (phase) {
        case 0: operation.begin(state->_0, animation.filtered_intents); break;
        case 1: {
            //82BB17D0 reads live ISkaterAnim v12/v28; earlier graph
            //operations can change these flags during the same traversal.
            S3_MHOST_SOME(const std::uint32_t animation_flags, animation.skater_animation_flags,
                           "FilterMotionGraphIntent requires live animation stance flags");
            operation.update(state->_0, animation.motion_intents, animation.filtered_intents, frame.dt,
                             {(animation_flags & 0x20000000u) != 0, (animation_flags & 0x40000000u) != 0});
            break;
        }
        default: operation.end(animation.filtered_intents); break;
    }
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion
