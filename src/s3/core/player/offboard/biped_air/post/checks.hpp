// Port of skate-core/src/player/offboard/biped_air/post/checks.rs (whole file).
// S3 82D8FEE8/82D90000; S2 82DD8C90/82DD8EF0 cross-check.
// These are Air's called checks, not replacements for general wipeout.
#pragma once
#include <array>

#include "s3/core/player/offboard/biped_air/height.hpp"
#include "s3/core/player/wipeout.hpp"

namespace s3::player::offboard::biped_air::recovered::post::checks {

using Vector = std::array<float, 4>;
using wipeout::Frame;
using wipeout::Mode;
using wipeout::Requests;

struct Thresholds {
    float squash = 0.0f;
    float displacement = 0.0f;
    float body_contact = 0.0f;
    float arm_contact = 0.0f;
};

struct Settings {
    Thresholds skeleton_air{};
    Thresholds offboard_air{};
    float offboard_min_speed = 0.0f;
};

namespace detail {
inline void collision(Requests& requests, Thresholds thresholds, Mode mode, const Frame& frame) {
    using height::dot;
    // FF48/9009C: squash is first and gated by the actual physics mode.
    if (mode.check_squash && frame.maximum_pose_error > thresholds.squash) {
        requests.request(18, 0.0f);
    } else if (dot(frame.pose_error, frame.pose_error) <= thresholds.displacement * thresholds.displacement) {
        // FFA4/900EC uses <=; retain its unordered-comparison behavior.
        if (wipeout::regional_force(frame, thresholds.body_contact, thresholds.arm_contact)) {
            requests.request(0, 0.0f);
        }
    } else {
        requests.request(1, 0.0f);
    }
}
}  // namespace detail

inline void skeleton_air(Requests& requests, const Settings& settings, Mode mode, const Frame& frame) {
    // 82D8FEE8 has neither a mode write nor a speed gate.
    detail::collision(requests, settings.skeleton_air, mode, frame);
}

inline void offboard_air(Requests& requests, const Settings& settings, Mode mode, const Frame& frame,
                         Vector root_velocity) {
    using height::dot;
    // 90018..20 occurs even below the speed threshold.
    requests.mode = 4;
    // 90028..5C reads Skeleton16336, not COM velocity.
    if (dot(root_velocity, root_velocity) > settings.offboard_min_speed * settings.offboard_min_speed) {
        detail::collision(requests, settings.offboard_air, mode, frame);
    }
}

}  // namespace s3::player::offboard::biped_air::recovered::post::checks
