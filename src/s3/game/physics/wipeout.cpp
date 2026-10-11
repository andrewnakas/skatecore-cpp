// Port of skate-game/src/physics/wipeout.rs
//! Shared Ground/Air wipeout requests from original TU3 82D8F9E0/82D90358.
#include "s3/game/physics/wipeout.hpp"

#include <sstream>
#include <vector>

#include "s3/game/physics.hpp"
#include "s3/game/physics/footplant.hpp"
#include "s3/game/physics/handplant.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"
#include "s3/game/state_names.hpp"

namespace s3::game::physics::wipeout {

namespace core_wipeout = ::s3::player::wipeout;
using ::s3::player::input_phase::ProcessedPhysicsInput;

Status Wipeout::check_air_collision(const ProcessedPhysicsInput& p, const core_wipeout::Frame& frame) {
    S3G_TRY(const core_wipeout::Mode m, mode(p));
    core_wipeout::check_air_collision(state, settings, m, frame);
    return ok();
}

rs::Result<Wipeout, std::string> Wipeout::load(const sd::Collections& data) {
    S3G_TRY(auto loaded, settings::load(data));
    core_wipeout::Requests state = core_wipeout::Requests::new_();
    state.initialize_player();  //Player82DB3024, after the component ctor.
    Wipeout w;
    w.state = state;
    w.settings = loaded.first;
    w.modes = loaded.second;
    return w;
}

Status Wipeout::check_ground(const Observations& input) {
    S3G_TRY(const core_wipeout::Mode m, mode(input.processed));
    S3G_TRY(const core_wipeout::Frame frame, input.frame());
    core_wipeout::check_ground(state, settings, m, frame);
    return ok();
}

Status Wipeout::check_ground_animation(const Observations& input, float scale) {
    S3G_TRY(const core_wipeout::Mode m, mode(input.processed));
    S3G_TRY(const core_wipeout::Frame frame, input.frame());
    core_wipeout::check_ground_animation(state, settings, m, frame, scale);
    return ok();
}

Status Wipeout::check_air(const Observations& input, bool use_com) {
    S3G_TRY(const core_wipeout::Mode m, mode(input.processed));
    S3G_TRY(const core_wipeout::Frame frame, input.frame());
    core_wipeout::check_air(state, settings, m, frame, use_com);
    return ok();
}

rs::Result<core_wipeout::Mode, std::string> Wipeout::mode(const ProcessedPhysicsInput& p) const {
    const std::size_t index = std::size_t(p.state_variant_index_2528);
    if (index < modes.size()) return modes[index];
    return rs::make_err(fmt::cat("Undefined wipeout physics mode ", p.state_variant_index_2528));
}

bool Wipeout::requests_runout(const ProcessedPhysicsInput& p) const {
    return state.requests_runout(observations::request_input(p));
}

bool Wipeout::requests_wipeout(const ProcessedPhysicsInput& p) const {
    return state.requests_wipeout(observations::request_input(p));
}

namespace {
template <class C>
std::string debug_floats(const C& c) {
    std::string s = "[";
    bool first = true;
    for (float v : c) {
        if (!first) s += ", ";
        s += fmt::debug_f32(v);
        first = false;
    }
    return s + "]";
}
}  // namespace

///Player postphysics82BD83E0 follows contact/error publication with the
///selected state's check, before FootIK sees that tick's wipeout request.
Status check_after_physics(const GamePhysics& physics, SkaterRuntime& skater) {
    using ::s3::player::state::PhysicalStateId;
    const Observations observations{
        skater.player_input.processed,
        physics.riding.ground,
        skater.collision_feedback,
        solve::deck_frame(physics.board),
        //Ground82BDF694..6A4 stores this target into Processed0..48;
        //Animated/KnownAir publish the same unblended target before board drive.
        skater.animated_skeleton.board_frames.animation_target,
        skater.animated_skeleton.roots.world_to_animation,
        skater.collision_pose_error,
        skater.collision_maximum_error,
        skater.player_state.post.jump_fix_frames,
        skater.air_reckoning.state,
        physics.riding.reckoning_frames.system[1][1],
        skater.trajectory.selector.grind_locked_to_middle(),
        skater.trajectory.selector.grind_normal(),
    };
    switch (skater.player_state.current()) {
        case PhysicalStateId::FootPlant:
        case PhysicalStateId::HandPlant: {
            S3G_TRY(const core_wipeout::Frame frame, observations.frame());
            if (skater.player_state.current() == PhysicalStateId::HandPlant) {
                handplant::trace_solved(physics, skater);
                //HandPlant vtable823273CC slot40 ->82D4C530 ->82D90358(false).
                //FootPlant's82D4C6D8 alone uses82D8FDC0. Its low trick-impact
                //thresholds must not replace HandPlant's normal air checks.
                S3G_CHECK(skater.wipeout.check_air(observations, false));
            } else {
                core_wipeout::check_plant(skater.wipeout.state, skater.wipeout.settings, frame);
            }
            if (skater.wipeout.state.count != 0) {
                std::string reasons = "[";
                bool first = true;
                for (std::size_t i = 0; i < skater.wipeout.state.reasons.size(); ++i) {
                    if (!skater.wipeout.state.reasons[i]) continue;
                    if (!first) reasons += ", ";
                    reasons += std::to_string(i);
                    first = false;
                }
                reasons += "]";
                std::string w2a = "[";
                for (std::size_t i = 0; i < 4; ++i) {
                    if (i) w2a += ", ";
                    w2a += debug_floats(frame.world_to_animation[i]);
                }
                w2a += "]";
                log::info(fmt::cat("PLANT_BAIL tick=", physics.ticks, " state=", debug_name(skater.player_state.current()),
                                   " reasons=", reasons, " pose_error=", debug_floats(frame.pose_error),
                                   " max_pose_error=", fmt::display_f32(frame.maximum_pose_error),
                                   " regions=", debug_floats(frame.regions_force),
                                   " closing=", debug_floats(frame.closing_velocity),
                                   " board_contact=", frame.board_contact, " world_to_animation=", w2a));
            }
            if (skater.player_state.current() == PhysicalStateId::FootPlant) {
                footplant::ground::post_physics(skater);
            }
            return ok();
        }
        case PhysicalStateId::PhysicsGround:
        case PhysicalStateId::SlideGround:
        case PhysicalStateId::RevertGround:
            return skater.wipeout.check_ground(observations);
        case PhysicalStateId::GroundAnimation:
            return skater.wipeout.check_ground_animation(observations, 1.0f);
        case PhysicalStateId::PhysicsAir:
        case PhysicalStateId::PhysicsAirSecondary:
            return skater.wipeout.check_air(observations, false);
        default:
            return ok();  //Other concrete states dispatch their own postphysics check.
    }
}

}  // namespace s3::game::physics::wipeout
