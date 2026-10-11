// Port of simref sim/src/camera/graph_subject.rs (skate-game camera/graph_subject.rs).
//! Remaining physical subject getters consumed by the stock camera graph.
//! These are publications from physics/animation, not deductions from the shot.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace s3::game::camera {

struct CameraGraphSubject {
    float time_since_player_input = 0.0f;  // getter520
    std::uint32_t wipeout_tweak = 0;       // 536
    bool onboard_air = false;              // 572
    bool preparing_to_jump = false;        // 576
    bool manual = false;                   // 580
    bool offboard_air = false;             // 612
    bool running_out = false;              // 616
    bool footplant = false;                // 620
    bool handplant = false;                // 624
    bool hippy_jump = false;               // 628
    bool hippy_hurdle = false;             // 632
    bool boneless = false;                 // 636
    bool moving_object = false;            // 644
    bool skitching = false;                // 648
    bool dropping_in = false;              // 664
    /// Physical output bundle2+184, read directly by82BB1EE0.
    float slow_motion_air_duration = 0.0f;
};

/// Engine-owned camera preferences and authored world camera-region membership.
/// An empty custom world has no road/ledge/volume camera annotations.
struct CameraGraphEnvironment {
    /// Normal stock camera type0=Low, type1=High (graph-authored selection).
    std::uint32_t camera_type = 0;
    bool on_road = false;
    bool ledge_left = false;
    bool ledge_right = false;
    std::vector<std::string> volumes;
};

}  // namespace s3::game::camera
