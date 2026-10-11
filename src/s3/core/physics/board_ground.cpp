// Port of skate-core/src/physics/board_ground.rs.
#include "s3/core/physics/board_ground.hpp"

#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::board_ground {

using board_motion_output::add;
using board_motion_output::dot;
using board_motion_output::inverse_length_squared;
using board_motion_output::length;
using board_motion_output::scale;
using board_motion_output::subtract;
using board_step::CollisionBody;

std::array<WheelLine, 4> wheel_lines(const board_runtime::BoardRuntime& board, Vector3 reckoning_up) {
    const auto poses = board.part_transforms();
    const Vector3 delta = scale(reckoning_up, WHEEL_LINE_LENGTH);
    std::array<WheelLine, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) {
        out[i] = WheelLine{poses[i].translation, subtract(poses[i].translation, delta)};
    }
    return out;
}

void WheelLineState::publish(std::array<std::optional<WheelLineHit>, 4> hits) {
    minimum_distance = WHEEL_LINE_LENGTH;
    for (std::size_t i = 0; i < 4; ++i) {
        audio_surfaces[i] = 0;
        physics_surfaces[i] = 0;
        seam_patterns[i] = 0;
        if (hits[i]) {
            const WheelLineHit hit = *hits[i];
            const float distance = hit.fraction * WHEEL_LINE_LENGTH;
            minimum_distance = distance - minimum_distance >= -0.0f ? minimum_distance : distance;
            normals[i] = hit.normal;
            distances[i] = distance;
            audio_surfaces[i] = hit.surface_tag & 0x7fu;
            physics_surfaces[i] = (hit.surface_tag >> 7) & 31u;
            seam_patterns[i] = (hit.surface_tag >> 12) & 0xfu;
        }
    }
}

void BoardGroundState::sample_accelerations(std::array<Vector3, BODY_COUNT> velocities, float time_step) {
    const float inverse_dt = 1.0f / time_step;
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        const Vector3 current = velocities[i];
        accelerations[i] = scale(subtract(current, previous_velocities[i]), inverse_dt);
        previous_velocities[i] = current;
    }
}

void BoardGroundState::advance_contact_time(float time_step) {
    time_without_wheel_contact = wheel_contact_count == 0 ? time_without_wheel_contact + time_step : 0.0f;
}

void BoardGroundState::update(Slice<const BoardContactReport> reports, const WheelLineState& lines,
                              Vector3 reckoning_up, float maximum_ground_angle_degrees, bool board_wiping_out) {
    parts.fill(EMPTY_PART);
    // CollisionInfo Reset82C00CA0 clears per-frame impacts and high flags.
    closing_velocity = Vector3::ZERO;
    maximum_closing_speed = 0.0f;
    surface_twelve_height = 0.0f;
    collision_flags &= 0x01ffffffu;
    part_audio_surfaces = {0, 0, 0};
    part_seam_patterns = {0, 0, 0};
    overall_normal = UP;
    valid_wheel_normals.fill(true);
    float highest_y = -2.0f;
    std::optional<std::size_t> highest_part;
    // 82C07E24..30 seeds min/max; only deck reports enter82C07EAC..CC.
    float minimum_projection = 1.0f;
    float maximum_projection = -1.0f;
    std::array<std::uint32_t, BODY_COUNT> surfaces{};
    for (std::size_t i = 0; i < 4; ++i) surfaces[i] = lines.physics_surfaces[i];
    for (const BoardContactReport& report : reports) {
        if (!(report.other == CollisionBody::StaticWorld())) {
            S3_PANIC("dynamic object contact classification needs its recovered owner");
        }
        const std::size_t i = board::index(report.part);
        if (report.part == BodyId::Deck) {
            const float projection = dot(report.normal, reckoning_up);
            minimum_projection = minimum_projection - projection >= 0.0f ? projection : minimum_projection;
            maximum_projection = maximum_projection - projection >= 0.0f ? maximum_projection : projection;
        }
        // 82C07ED0..7F2C: this is the OLD part velocity, not the report's
        // post-solve relative velocity or the finite-difference acceleration.
        const float closing = -dot(report.normal, previous_velocities[i]);
        if (!(closing <= maximum_closing_speed)) {
            maximum_closing_speed = closing;
            closing_velocity = scale(report.normal, -closing);
        }
        const std::uint32_t surface = (static_cast<std::uint32_t>(report.other_surface) >> 7) & 31u;
        if (surface == 12) {
            collision_flags |= 1u << 25;  // 82C080D4..80F0.
            surface_twelve_height = report.position.y;
        }
        if (i >= 4) {
            surfaces[i] = surface;  // 82C081B4..81E0, last report wins.
            // The same stores keep the audio material and seam bits of the tag.
            const std::uint32_t tag = static_cast<std::uint32_t>(report.other_surface);
            part_audio_surfaces[i - 4] = tag & 0x7fu;
            part_seam_patterns[i - 4] = (tag >> 12) & 0xfu;
        }
        PartGroundContact& contact = parts[i];
        if (!contact.in_contact || report.normal.y > contact.normal.y) {
            contact.normal = report.normal;
            contact.point = report.position;
            contact.relative_velocity = report.relative_linear_velocity;
        }
        // Selection occurs after the per-part maximum, in report order.
        if (contact.normal.y > highest_y) {
            highest_y = contact.normal.y;
            highest_part = i;
        }
        contact.in_contact = true;
    }
    // 82C082AC/E8/F4/F8; no deck reports produce max(-2,0)=0.
    const float range = maximum_projection - minimum_projection;
    opposing_contact = -range >= 0.0f ? 0.0f : range;
    // 82C0834C..836C combines physical contact with the wheel query's
    // surface (wheels), or the last actual contact tag (trucks/deck).
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        if (parts[i].in_contact && surfaces[i] == 8) {
            collision_flags |= 1u << 31;
        }
    }
    // The source's -1 best-part index addresses the retained wheel normal.
    const Vector3 reference_normal = highest_part ? parts[*highest_part].normal : wheel_normal;
    for (std::size_t i = 0; i < 4; ++i) {
        if (!parts[i].in_contact) {
            if (lines.distances[i] < rs::from_bits(0x3D8F5C29u)) {
                parts[i].normal = lines.normals[i];
            } else {
                valid_wheel_normals[i] = false;
            }
        }
    }
    std::uint8_t part_count = 0;
    for (const auto& part : parts) part_count += part.in_contact ? 1 : 0;
    part_contact_count = part_count;
    std::uint8_t wheel_count = 0;
    for (std::size_t i = 0; i < 4; ++i) wheel_count += parts[i].in_contact ? 1 : 0;
    wheel_contact_count = wheel_count;
    const float angle_radians = maximum_ground_angle_degrees * rs::from_bits(0x3C8EFA35u);
    const float minimum_up_dot = trigonometry::cos(angle_radians);
    Vector3 sum = Vector3::ZERO;
    for (std::size_t i = 0; i < 4; ++i) {
        const PartGroundContact contact = parts[i];
        if (valid_wheel_normals[i] && dot(contact.normal, reckoning_up) > minimum_up_dot &&
            angle_between(reference_normal, contact.normal) < rs::from_bits(0x3F490FDBu)) {
            sum = add(sum, contact.normal);
        }
        const float drag = contact.in_contact ? (board_wiping_out ? rs::from_bits(0x3D23D70Au) : 0.0f)
                                              : rs::from_bits(0x3BC49BA6u);
        wheel_angular_drag[i] = drag * rs::from_bits(0x426FFFFFu);
    }
    const float squared = dot(sum, sum);
    if (wheel_contact_count > 0 && squared > rs::from_bits(0x37800000u)) {
        overall_normal = scale(sum, inverse_length_squared(squared, 2));
        wheel_normal = overall_normal;
    } else {
        Vector3 support = Vector3::ZERO;
        // Native sum order: deck, front truck, back truck.
        for (BodyId id : {BodyId::Deck, BodyId::FrontTruck, BodyId::BackTruck}) {
            const std::size_t i = board::index(id);
            if (parts[i].in_contact) {
                support = add(support, parts[i].normal);
            }
        }
        const float magnitude = length(support);
        if (magnitude > rs::from_bits(0x3C23D70Au)) {
            float inverse = native_arithmetic::reciprocal_estimate(magnitude);
            for (int k = 0; k < 2; ++k) {
                const float error = rs::mul_add(-inverse, magnitude, 1.0f);
                inverse = rs::mul_add(inverse, error, inverse);
            }
            overall_normal = scale(support, inverse);
        }
    }
}

float angle_between(Vector3 a, Vector3 b) {
    const float a_squared = dot(a, a);
    const float b_squared = dot(b, b);
    const float epsilon = rs::from_bits(0x38D1B717u);
    if (!(a_squared > epsilon && b_squared > epsilon)) {
        return 0.0f;
    }
    const Vector3 an = scale(a, inverse_length_squared(a_squared, 1));
    const Vector3 bn = scale(b, inverse_length_squared(b_squared, 1));
    const float cosine = native_arithmetic::vector_min(native_arithmetic::vector_max(dot(an, bn), -1.0f), 1.0f);
    return trigonometry::acos(cosine);
}

}  // namespace s3::physics::board_ground
