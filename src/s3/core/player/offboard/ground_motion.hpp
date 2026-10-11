// Port of skate-core/src/player/offboard/ground_motion.rs.
// Original TU3 Biped movement82D7EDC8 and leaves800C0/80EB0/80CE0.
// S3 SHA256431b8eba23565affdc10d137df19b06fe286244cefb3e1a13f32693e9600395a.
// The controller owns these fields once; this stage makes no collision queries.
#pragma once
#include <array>
#include <cstdint>
#include <utility>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::ground_motion {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;

/// Only fields written by this stage. Integrate these into the one Biped owner;
/// velocity480, correction576/711 and frame0 must not become stale shadow copies.
struct GroundMotionState {
    Frame frame_0{};
    Frame published_frame_64{};
    Frame previous_support_frame_192{};
    Vector support_velocity_256{};
    Vector predicted_support_velocity_272{};
    Vector previous_support_velocity_288{};
    Vector support_acceleration_304{};
    Vector filtered_local_acceleration_320{};
    float support_yaw_336 = 0.0f;
    float previous_support_yaw_340 = 0.0f;
    float predicted_support_yaw_344 = 0.0f;
    float support_speed_348 = 0.0f;
    std::uint32_t support_id_352 = 0;
    Vector velocity_480{};
    Vector correction_576{};
    Frame target_frame_608{};
    float angular_velocity_688 = 0.0f;
    float speed_704 = 0.0f;
    bool correction_enabled_711 = false;
    bool support_velocity_removed_715 = false;
    float target_scale_784 = 0.0f;
};

/// Actual job values and external-controller fields, never query placeholders.
struct GroundMotionInput {
    Vector contact_position_0{};
    Frame contact_frame_32{};
    Vector contact_target_96{};
    Vector contact_normal_112{};
    std::uint32_t contact_flags_176 = 0;
    std::uint32_t contact_id_180 = 0;
    Vector animation_motion_224{};
    Vector animation_motion_240{};
    float requested_duration_288 = 0.0f;
    bool mirrored_292 = false;
    bool animation_directed_304 = false;
    bool target_frame_present_352 = false;
    Frame target_frame_368{};
    /// Externally produced Biped frame128..176; read by800C0 and angle limiting.
    Frame reference_frame_128{};
    /// Contact correction producer384 (not the job's frame384).
    Vector contact_displacement_384{};
    Vector velocity_addition_528{};
    Vector desired_up_544{};
    Vector correction_target_592{};
    Vector obstacle_target_672{};
    bool obstacle_enabled_713 = false;
};

/// 82D7EDC8. Order: support history, approach, displacement/heading, publication.
void update(GroundMotionState& state, const GroundMotionInput& input);

// Private submodules (exposed for the ported unit tests).
namespace approach {
std::pair<Vector, Vector> calculate(GroundMotionState& s, const GroundMotionInput& i, Vector position, const Frame& delta);
}
namespace publication {
/// 82D80CE0: published frame copy and retained positional correction decay.
void update(GroundMotionState& s, Vector displacement);
}
namespace support {
/// 82D7EEAC..F454 moving support history and800C0 output filtering.
Frame update(GroundMotionState& s, const GroundMotionInput& i);
}

}  // namespace s3::player::offboard::ground_motion
