// Port of skate-core/src/player/offboard/biped_air/sampling.rs (+ the SelectorState
// methods of selection.rs).
// Original offboard selector GetTrajectoryResultAtFrame82D6E3F8.
// This selector is distinct from the on-board TrajectorySelector82D67848.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/_deps/air_trajectory.hpp"
#include "s3/core/player/offboard/biped_air/result.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::player::offboard::biped_air::recovered {
namespace selection {
struct Candidate;
}
}  // namespace s3::player::offboard::biped_air::recovered

namespace s3::player::offboard::biped_air::recovered::sampling {

using air::trajectory::Prediction;
using air::trajectory::Trajectory;
using result::TrajectoryResult;
using Vector = std::array<float, 4>;

///Retained data produced by offboard selection82D6CC50 and adjustment82D6DE08.
///No Default in Rust: completing a scene query must supply these actual observations.
struct Selection {
    Trajectory trajectory_8144{};
    Vector normal_6144{};
    Vector velocity_6160{};
    Vector position_6176{};
    bool valid_6200 = false;
    bool result_present_3888 = false;
    std::int32_t landing_frame_8480 = 0;
    float scalar_8392 = 0.0f;
    std::uint32_t word_8396 = 0;
    ///Selected candidate+100, or launch packet+100 when index8476 is -1.
    float candidate_scalar_100 = 0.0f;
};

struct SelectorState {
    bool pending_8492 = false;
    bool restart_allowed_8493 = false;
    bool preinitialized_8494 = false;
    Trajectory fallback_8208{};
    Selection selection{};
    Vector adjustment_8336{};
    float blend_8384 = 0.0f;
    float elapsed_8388 = 0.0f;
    std::int32_t frame_8484 = 0;

    ///The game constructs this from the actual launch/selection producer.
    void sample(std::int32_t frame, float timestep, const point_graph::PointGraph<8>& blend, TrajectoryResult& out);

    // ---- selection.rs ----
    ///82D6BD78's sample-visible reset. Packet3904 is NOT reset by that function;
    ///when index8476 is -1, sampling still reads its retained scalar4004.
    static SelectorState reset_sampling(float retained_launch_scalar_100);
    ///82D6CA58's fallback seed. Call once per actual launch, never per Update.
    void seed_fallback(Vector position, Vector velocity, Vector gravity);
    ///Commit the candidate chosen by82D6D020/CC50. Return native byte8497's
    ///set event; a miss does not clear that retained flag or8392/8396.
    bool commit(const selection::Candidate& candidate, Prediction& prediction, Vector offset_8272);
};

}  // namespace s3::player::offboard::biped_air::recovered::sampling
