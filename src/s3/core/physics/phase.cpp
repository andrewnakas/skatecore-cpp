// Port of skate-core/src/physics/phase.rs.
#include "s3/core/physics/phase.hpp"

namespace s3::physics::phase {

rs::Result<rs::Unit, std::string> PhysicsCommandBuffer::push(std::uint64_t tick,
                                                            PhysicsCommand command) {
    if (tick != tick_) {
        return rs::make_err("Physics command belongs to tick " + std::to_string(tick) +
                            ", buffer owns tick " + std::to_string(tick_));
    }
    commands_.push_back(command);
    return rs::Unit{};
}

rs::Result<rs::Unit, std::string> PhysicsCommandBuffer::clear(std::uint64_t tick) {
    if (tick != tick_) {
        return rs::make_err("Cannot clear physics commands for tick " + std::to_string(tick) +
                            "; buffer owns tick " + std::to_string(tick_));
    }
    commands_.clear();
    return rs::Unit{};
}

rs::Result<rs::Unit, std::string> PhysicsEventBuffer::emit(std::uint64_t tick, PhysicsEvent event) {
    if (tick != tick_) {
        return rs::make_err("Physics event belongs to tick " + std::to_string(tick) +
                            ", buffer owns tick " + std::to_string(tick_));
    }
    events_.push_back(event);
    return rs::Unit{};
}

}  // namespace s3::physics::phase
