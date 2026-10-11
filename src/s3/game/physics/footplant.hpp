// Port of skate-game/src/physics/footplant.rs.
//! FootPlantManager state, original TU3 inline ctor82DB217C and Reset82D6F440.
//! Query results are retained separately from pending query ownership.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/air/known.hpp"
#include "s3/core/air/trajectory.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/footplant/ground.hpp"
#include "s3/game/physics/footplant/math.hpp"
#include "s3/game/physics/footplant/settings.hpp"
#include "s3/game/physics/footplant/update.hpp"

namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;  // physics/animated_skeleton.hpp (group D)
}
namespace s3::game::physics::foot_ik {
struct FootIk;  // physics/foot_ik.hpp (group D)
}

namespace s3::game::physics::footplant {

using settings::Settings;
using update::FootplantFrame;
using V = std::array<float, 4>;
using sc::air::trajectory::QueryResult;
using sc::air::trajectory::Trajectory;

struct Footplant {
    bool enabled = false;  //240; FullReset and Air Enter clear this separately.
    QueryResult result = QueryResult::miss();
    Settings settings{};
    std::optional<std::size_t> selected_toe;  //616 signed -1
    bool candidate = false;                   //624
    bool hit = false;                         //625
    bool perform = false;                     //626
    bool flag_627 = false;
    bool launch_valid = false;    //628
    bool lock_valid = false;      //629
    bool contact_active = false;  //630
    bool requested = false;       //631
    float contact_time = 0.0f;    //592
    float scalar_596 = 0.0f;
    float scalar_600 = 0.0f;
    float target_blend = 0.0f;    //604
    float active_elapsed = 0.0f;  //608
    float scalar_612 = 0.0f;
    std::uint32_t surface = 0;    //620 survives Reset
    V contact{};                  //288
    V adjusted_contact{};         //304
    V current_up{};               //320: Reset preserves it; ctor does not define it
    V selected_world{};           //256
    V selected_record{};          //272
    V leg_direction{};            //336
    std::array<V, 2> vectors_352_368{};
    std::array<V, 4> curve{};               //448/464/480/496
    V physical_com{};                       //384
    V animation_com{};                      //400
    V launch_direction{};                   //416
    V locked_target{};                      //432
    Trajectory request{};                   //528..591
    Trajectory completed_trajectory{};      //query144, retained with result

    static rs::Result<Footplant, std::string> load(const sd::Collections& data);
    ///82D6F440. Does not cancel the query or erase its last delivered result,
    ///current up, selected positions, or surface. Enter clears240 separately.
    void reset();
    void full_reset();
    /// Common ProcessOutput82DB71B0 calls82D71040 before selected state Fill.
    void publish(sc::player::input_phase::AirOutputFields& out) const;

    // ---- footplant/update.rs ----
    void update_candidate(const sc::air::known::KnownAirFootplantInput& input, const FootplantFrame& frame);
    void update_launch(const sc::air::known::KnownAirFootplantInput& input);
    Status consume_and_submit(const sc::air::known::KnownAirFootplantInput& input, const FootplantFrame& frame,
                              foot_ik::FootIk& ik, sc::physics::skeleton_body::SkeletonCollisionMode& collision);
    void clear_contact();
    ///82D6FBB0: query the real edge geometry within the native two-metre box.
    void nearby_edge(const FootplantFrame& frame);

    // ---- footplant/ground.rs ----
    std::uint32_t start(std::array<float, 4> com, std::array<float, 4> velocity);
    ///82D70CE0: authored BodyAdjustX/Z move the release point, retaining
    ///the last handle length.82C1E170 removes only positive up projection.
    void adjust(float t, std::array<float, 2> input);

    // ---- footplant/pose.rs ----
    void update_pose(const sc::air::known::KnownAirFootplantInput& input,
                     const animated_skeleton::AnimatedSkeleton& skeleton, foot_ik::FootIk& ik,
                     sc::physics::skeleton_body::SkeletonCollisionMode& collision);
};

}  // namespace s3::game::physics::footplant
