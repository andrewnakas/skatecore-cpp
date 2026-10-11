// Port of skate-core/src/player/offboard/controller/state.rs (whole file).
#pragma once
#include <array>
#include <optional>

#include "s3/core/player/offboard/cadence.hpp"
#include "s3/core/player/offboard/contact_correction.hpp"
#include "s3/core/player/offboard/controller/output.hpp"
#include "s3/core/player/offboard/ground_motion.hpp"
#include "s3/core/player/offboard/movement_intent.hpp"
#include "s3/core/player/offboard/position_output.hpp"
#include "s3/core/player/offboard/slide.hpp"
#include "s3/core/player/offboard/surface_frame.hpp"

namespace s3::player::offboard::controller {

// pub(super)
inline constexpr Vector ZERO{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Vector UP{0.0f, 1.0f, 0.0f, 0.0f};
inline constexpr Frame IDENTITY{{{1.0f, 0.0f, 0.0f, 0.0f}, UP, {0.0f, 0.0f, 1.0f, 0.0f}, ZERO}};

struct ClipMetric {
    float translation_z = 0.0f;
    float end_time = 0.0f;
    float speed() const { return (-1.0f / end_time) * translation_z; }
};

struct PlacementInput;

/// Each persistent original field has one owner. Per-stage packets in step are
/// temporary and copy updated shared fields back before the next producer.
struct State {
    ground_motion::GroundMotionState motion;
    movement_intent::State intent;
    surface_frame::State surface;
    contact_correction::ContactCorrection contact;
    slide::SpecialMode special;
    slide::Sliding sliding;
    position_output::FrameOutput frame_output;
    cadence::BipedCadence cadence;
    cadence::CadenceThresholds thresholds;
    Vector position_368{};
    Vector correction_target_592{};
    float forward_delta_692 = 0.0f;
    float right_delta_696 = 0.0f;
    float velocity_override_remaining_772 = 0.0f;
    bool alternate_709 = false;

    static State new_(const std::array<std::optional<ClipMetric>, 3>& metrics);
    ///82D7B1C0 deliberately leaves phase716..736 and vectors576/592 alone.
    void reset();
    // placement.rs
    void place(const PlacementInput& i);

private:
    void clear_entry_correction();
};

}  // namespace s3::player::offboard::controller
