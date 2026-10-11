// Port of skate-game/src/graph_host/motion_manual.rs (module `motion_manual`).
#include "s3/game/graph_host/motion_manual.hpp"

#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_manual {

rs::Result<s3::animation::manual::Settings, std::string> settings(const s3::data::Collections& data) {
    S3_MOTION_TRY(const auto words, motion_collections::words<16>(data, "anim_motion", "manual", "manual_balance"));
    s3::animation::manual::Settings s;
    s.balance = motion_collections::graph<8>(words, 0, 8);
    S3_MOTION_TRY(s.velocity_limit, motion_collections::float_(data, "anim_motion", "manual", "manual_clamp_vel"));
    S3_MOTION_TRY(s.acceleration_limit,
                  motion_collections::float_(data, "anim_motion", "manual", "manual_clamp_acc"));
    return s;
}

}  // namespace s3::game::graph_host::motion_manual
