// Port of skate-core/src/player/offboard/ground_job.rs (whole file).
// Ground-state PreUpdate82D30D30, prefix copy82D31040 and geometry82D31620.
// Original Skate3 SHA256:
// 431b8eba23565affdc10d137df19b06fe286244cefb3e1a13f32693e9600395a.
// Skate2 locomotion82DA0678 uses FeetIK/Skeleton, not this Biped job contract.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/math.hpp"
#include "s3/core/player/offboard/contact_toolkit.hpp"
#include "s3/core/player/offboard/controller.hpp"
#include "s3/core/player/offboard/ground_entry.hpp"
#include "s3/core/player/offboard/ground_input.hpp"
#include "s3/core/player/offboard/ground_query.hpp"

namespace s3::player::offboard::ground_job {

using contact_toolkit::ContactPrefix;
using controller::Frame;
using controller::GroundJob;
using controller::Vector;
using ground_input::GroundInputOutput;
using ground_query::ConsumeInput;
using ground_query::GroundAdjustment;

inline const float STEP = rs::from_bits(0x3c888889u);

/// The shared toolkit owner publishes BOTH fields, including its reset prefix
/// when refresh completes without a submitted batch. Absence is not readiness.
struct ContactSnapshot {
    std::int32_t readiness = 0;
    ContactPrefix prefix{};
};

struct Input {
    ContactSnapshot contact{};
    Frame frame{};
    std::uint32_t previous_state = 0;
    /// Only processed third-line1056 with its actual valid byte1092 nonzero.
    std::optional<Vector> third_line_position;
    std::uint32_t frames_since_teleport = 0;
    Vector processed_position{};
    Vector processed_velocity{};
    GroundInputOutput controls{};
    std::array<Vector, 2> collision_displacements{};
    Vector animation_motion{};
    Vector animation_velocity{};
    float requested_duration = 0.0f;
    float requested_phase = 0.0f;
    float override_duration = 0.0f;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t flags_2488 = 0;
};

struct Prepared {
    GroundJob job;
    /// Includes state754, retained for the physical state's later consumers even
    /// though the existing controller does not read that byte.
    GroundAdjustment geometry;
};

namespace detail {
math::Vector3 xyz(Vector v);
ground_query::Frame query_frame(const Frame& f);
Frame native_frame(const ground_query::Frame& f);
}  // namespace detail

/// Retained contact192 and timer164 belong to the EXISTING Ground state.
/// The callback `consume_geometry(ConsumeInput) -> rs::Result<GroundAdjustment, E>`
/// consumes the preceding geometry submission exactly once, after contact
/// selection and before controller execution. Errors are never support.
template <class E, class F>
rs::Result<Prepared, E> prepare(ContactPrefix& retained, float& timer_164, const Input& input, F&& consume_geometry) {
    timer_164 += STEP;
    // r27 starts at1 (82D30D54); only the ready branch recomputes both-feet.
    bool both_feet = true;
    if (input.contact.readiness > 0) {
        retained = input.contact.prefix;
        both_feet = (retained.flags_176 & 0x30u) == 0x30u;
    } else if (input.previous_state != 501) {
        if (input.third_line_position) {
            // 82D30DC8..DE4 changes ONLY these three fields.
            retained.flags_176 = 1;
            retained.position = *input.third_line_position;
            retained.normal = input.frame[1];
        }
    }
    // 82D30DF0..E04: carry/sign sequence is signed <20, including high-bit words.
    const bool suppress_minimum = static_cast<std::int32_t>(input.frames_since_teleport) < 20;
    if (suppress_minimum) {
        retained.flags_176 &= ~8u;
    }
    // 82D30EA4 copies before geometry changes ONLY the job's normal at416.
    const ContactPrefix copied = retained;
    ConsumeInput ci;
    ci.frame_80 = detail::query_frame(input.frame);
    ci.contact_position_192 = detail::xyz(retained.position);
    ci.contact_flags_368 = retained.flags_176;
    ci.reach_364 = retained.distance_172;
    ci.previous_input_up_416 = detail::xyz(copied.normal);
    rs::Result<GroundAdjustment, E> geometry_r = consume_geometry(ci);
    if (geometry_r.is_err()) return rs::make_err(geometry_r.unwrap_err());
    const GroundAdjustment geometry = geometry_r.unwrap();
    if (!both_feet && !geometry.state_753 && (retained.flags_176 & 8u) == 0) {
        timer_164 = 0.0f;
    }
    // VMX82D30F48: position592 + velocity608 * float1/60, not render/solver dt.
    const Vector animation_position = rs::from_fn<4>(
        [&](std::size_t i) { return rs::mul_add(input.processed_velocity[i], STEP, input.processed_position[i]); });
    GroundJob job;
    job.contact_position = copied.position;
    job.contact_normal = {geometry.input_up_416.x, geometry.input_up_416.y, geometry.input_up_416.z, copied.normal[3]};
    job.support_frame = copied.support_frame;
    job.target_position = copied.target_position;
    job.target_normal = copied.target_normal;
    job.edge_position = copied.edge_position;
    job.edge_normal = copied.edge_normal;
    job.flags = copied.flags_176;
    job.support_id = copied.support_180;
    job.collision_displacements = input.collision_displacements;
    job.animation_motion = input.animation_motion;
    job.animation_velocity = input.animation_velocity;
    job.desired_direction = input.controls.state_656;
    job.animation_position = animation_position;
    job.requested_duration = input.requested_duration;
    job.mirrored = (input.flags_2476 & 4u) != 0;
    job.requested_phase = input.requested_phase;
    job.override_duration = input.override_duration;
    job.animation_directed = (input.flags_2488 & 0x08000000u) != 0;
    job.movement = input.controls.state_708;
    job.steering = input.controls.state_712;
    job.sprint_pressed = (input.flags_2484 & 0x00020000u) != 0;
    job.suppress_lean = (input.flags_2480 & 0x80u) != 0;
    job.suppress_minimum = suppress_minimum;
    job.target_frame_present = geometry.state_752;
    job.edge_active = geometry.state_753;
    job.target_frame = detail::native_frame(geometry.frame_768);
    job.ignore_obstacle = (input.flags_2472 & 0x10000000u) != 0;
    return Prepared{job, geometry};
}

/// Frame-producing portion of Sync82D31E64..82D32070. The same Ground state is
/// updated in place; callers use this frame for Skeleton AND toolkit inputs.
Frame sync_frames(ground_entry::State& state, const ContactPrefix& contact, const controller::GroundResult& result);

}  // namespace s3::player::offboard::ground_job
