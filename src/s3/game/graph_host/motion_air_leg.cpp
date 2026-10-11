// Port of skate-game/src/graph_host/motion_air_leg.rs (module `motion_air_leg`).
#include "s3/game/graph_host/motion_air_leg.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_air_leg {

using s3::animation::skeleton_input::name::encode;

Operation Operation::parse(const Attributes& a) {
    const std::string* height = a.text("driveDistToCom");
    const std::string* bone_ptr = a.text("bone");
    const std::string bone = bone_ptr ? *bone_ptr : std::string("Skateboard_Root");
    return Operation{encode(height ? *height : std::string("disttocog")),
                     bone == "LeftToeBase"    ? Bone::LeftToe
                     : bone == "RightToeBase" ? Bone::RightToe
                                              : Bone::Board};
}

rs::Result<Settings, std::string> load(const s3::data::Collections& data) {
    auto value = [&](const char* name) {
        return motion_collections::float_(data, "anim_motion", "inair_disttocog", name);
    };
    //PointNegGraphData4 includes four metadata words before x/y. Update uses
    //82481E10 on the four authored knots, not the negative-graph evaluator.
    S3_MOTION_TRY(const auto w,
                  motion_collections::words<12>(data, "anim_motion", "inair_disttocog", "lo_air_arm_extend"));
    Settings s;
    S3_MOTION_TRY(s.going_up_speed, value("goingup_disttocog_speed"));
    S3_MOTION_TRY(s.going_down_speed, value("goingdown_disttocog_speed"));
    S3_MOTION_TRY(s.minimum_height, value("min_inair_disttocog"));
    S3_MOTION_TRY(s.preland_velocity, value("preland_disttocom_vel"));
    S3_MOTION_TRY(s.preland_final_height, value("preland_final_disttocom"));
    S3_MOTION_TRY(s.offboard_multiplier, value("offboard_preland_disttocom_vel_multiplier"));
    s.arm_extension = motion_collections::graph<4>(w, 4, 8);
    return s;
}

}  // namespace s3::game::graph_host::motion_air_leg
