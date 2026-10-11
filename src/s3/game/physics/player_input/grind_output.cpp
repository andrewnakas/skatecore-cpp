// Port of skate-game/src/physics/player_input/grind_output.rs.
//! Completed common/Skeleton output needed by the stock grind graph.
#include "s3/game/physics/player_input/grind_output.hpp"

namespace s3::game::physics::player_input {

Status PlayerInputRuntime::publish_grind_graph_outputs(const sc::physics::skeleton_body::SkeletonPhysicalRecord& record,
                                                       std::array<float, 4> reckoning_up,
                                                       const sc::air::trajectory::TrajectorySelector& selector) {
    if (!toolkit.has_value()) {
        return rs::make_err(std::string("Grind output requires the completed physical input toolkit"));
    }
    const auto& t = *toolkit;
    // Common ProcessOutput82DB703C copies selector9653 unconditionally.
    physical.air.flag_443 = selector.grind_locked_to_middle() ? 1 : 0;
    physical.skeleton.publish_twist(record, t.forward, reckoning_up);
    return ok();
}

}  // namespace s3::game::physics::player_input
