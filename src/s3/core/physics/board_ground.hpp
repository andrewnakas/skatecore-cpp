// Port of skate-core/src/physics/board_ground.rs.
//
// Board contact normals and wheel-line state used by riding output.
//
// TU3 CollisionInfo constructor82C00D68/reset82C00CA0, wheel result
// publication82C079E0, and world-contact normal selection82C07D20. The
// postphysics stage consumes actual solver reports, never speculative query
// contacts. Dynamic-object classification is outside the host's static world.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/math.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/contact_feedback.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::board_runtime {
class BoardRuntime;
}

namespace s3::physics::board_ground {

using board::BODY_COUNT;
using board::BodyId;
using contact_feedback::BoardContactReport;
using math::Vector3;

inline constexpr Vector3 UP = Vector3{0.0f, 1.0f, 0.0f};
inline constexpr float WHEEL_LINE_LENGTH = rs::from_bits(0x3E4CCCCDu);

struct WheelLine {
    Vector3 start;
    Vector3 end;
};
S3_DERIVE_EQ(WheelLine, start, end)

/// The four zero-radius queries in82C07788. Up is Reckoning+1152, supplied
/// by PhysicalPlayerHiLOD::StartSkateboardLineTests82DB6310.
std::array<WheelLine, 4> wheel_lines(const board_runtime::BoardRuntime& board, Vector3 reckoning_up);

struct WheelLineHit {
    /// Query result+96, before82C079E0 multiplies by the authored line length.
    float fraction = 0.0f;
    Vector3 normal;
    std::uint32_t surface_tag = 0;
};
S3_DERIVE_EQ(WheelLineHit, fraction, normal, surface_tag)

struct WheelLineState {
    // 82C00D68 initializes all four normal/distance/surface records.
    std::array<Vector3, 4> normals{UP, UP, UP, UP};
    std::array<float, 4> distances{};
    /// Low seven bits of the packed map material tag. This is the authored audio material and is
    /// deliberately distinct from the five-bit friction/physics surface below.
    std::array<std::uint32_t, 4> audio_surfaces{};
    std::array<std::uint32_t, 4> physics_surfaces{};
    /// Seam pattern of the same tag, `(tag >> 12) & 0xF`: `82C079E0` stores it at
    /// CollisionInfo+812+4i (FillPhysOut `82C02A80` copies it to Collision+3456+4i, audio state
    /// +636..+648). Cleared to 0 on a miss together with both surfaces.
    std::array<std::uint32_t, 4> seam_patterns{};
    float minimum_distance = 0.0f;

    /// `impl Default` (same values as the member initializers).
    static WheelLineState default_() { return WheelLineState{}; }

    /// Complete four-wheel branch82C079F8..82C07A9C. A miss clears the
    /// surface, while the previous distance and normal deliberately survive.
    void publish(std::array<std::optional<WheelLineHit>, 4> hits);
};
S3_DERIVE_EQ(WheelLineState, normals, distances, audio_surfaces, physics_surfaces, seam_patterns, minimum_distance)

struct PartGroundContact {
    bool in_contact = false;
    Vector3 normal;
    Vector3 point;
    Vector3 relative_velocity;
};
S3_DERIVE_EQ(PartGroundContact, in_contact, normal, point, relative_velocity)

inline constexpr PartGroundContact EMPTY_PART = PartGroundContact{false, UP, Vector3{}, Vector3{}};

struct BoardGroundState {
    std::array<PartGroundContact, BODY_COUNT> parts{EMPTY_PART, EMPTY_PART, EMPTY_PART, EMPTY_PART,
                                                    EMPTY_PART, EMPTY_PART, EMPTY_PART};
    /// BoardBody320..416, retained between postphysics updates.
    // CollisionInfo ctor82C00DC8/CC zeros both arrays. Board reset
    // 82C0D758 calls that constructor again; per-frame reset retains velocities.
    std::array<Vector3, BODY_COUNT> previous_velocities{};
    /// BoardBody432..528, observed accelerations in actual part order.
    std::array<Vector3, BODY_COUNT> accelerations{};
    /// Body656/852: strongest closing velocity against this frame's contacts,
    /// computed from retained part velocities before acceleration sampling.
    Vector3 closing_velocity;
    float maximum_closing_speed = 0.0f;
    /// Body860: range of DECK contact normal projections onto Reckoning1152.
    float opposing_contact = 0.0f;
    /// Body864/872: static-world contact classifications and surface12 height.
    /// Other-assembly group flags require actual dynamic-object reports.
    float surface_twelve_height = 0.0f;
    std::uint32_t collision_flags = 0;
    /// CollisionInfo+800/+804/+808: audio material (`tag & 0x7F`) of the front truck, back truck
    /// and deck. Reset `82C00CA0` clears them each frame; `82C07D20` (82C081B4..81E0) stores the
    /// tag of every report for parts 4..6, the last report winning. FillPhysOut `82C02A80` copies
    /// them to Collision+4/+8/+12 (audio state +652/+656/+660).
    std::array<std::uint32_t, 3> part_audio_surfaces{};
    /// CollisionInfo+828/+832/+836: the same reports' seam patterns, `(tag >> 12) & 0xF`.
    std::array<std::uint32_t, 3> part_seam_patterns{};
    /// CollisionInfo+0. Can use truck/deck normals when the wheel sum fails.
    Vector3 overall_normal = UP;
    /// CollisionInfo+16. Retained until a valid contacting-wheel sum replaces it.
    /// FillPhysOut publishes this to Ground+80 and Ground+96.
    Vector3 wheel_normal = UP;
    std::array<bool, 4> valid_wheel_normals{};
    std::uint8_t part_contact_count = 0;
    std::uint8_t wheel_contact_count = 0;
    /// Body7692, accumulated only while all four physical wheels lack contact.
    float time_without_wheel_contact = 0.0f;
    /// Angular drag: S3 postphysics82C08634 writes wheel inertia+36;
    /// paired S2 postphysics82B372E0 names this mAngularDrag.
    std::array<float, 4> wheel_angular_drag{};

    /// `impl Default` (same values as the member initializers).
    static BoardGroundState default_() { return BoardGroundState{}; }

    /// Original PostPhysics82C082B8..8444. The scalar reciprocal timestep is
    /// shared by all seven parts, then each previous velocity is replaced.
    void sample_accelerations(std::array<Vector3, BODY_COUNT> velocities, float time_step);

    /// 82C087D8..87F4 after contact publication. The incoming f1 is the
    /// actual physics timestep; reset82C0D680 deliberately preserves this field.
    void advance_contact_time(float time_step);

    /// Complete contact-normal/count/drag subpipeline of82C07D20 for the
    /// static world. Reports must already satisfy82AE1608/827682B0 eligibility.
    /// `maximum_ground_angle_degrees` is physics_reckoning.
    /// MaxAllowedGroundNormalFromUp (full64 0E508708CC7F5249).
    void update(Slice<const BoardContactReport> reports, const WheelLineState& lines, Vector3 reckoning_up,
                float maximum_ground_angle_degrees, bool board_wiping_out);
};
S3_DERIVE_EQ(BoardGroundState, parts, previous_velocities, accelerations, closing_velocity, maximum_closing_speed,
             opposing_contact, surface_twelve_height, collision_flags, part_audio_surfaces, part_seam_patterns,
             overall_normal, wheel_normal, valid_wheel_normals, part_contact_count, wheel_contact_count,
             time_without_wheel_contact, wheel_angular_drag)

/// Complete8296EBB0, including its small-vector gate and ONE reciprocal-square-
/// root refinement per vector. Native82C07D20 compares this angle with pi/4.
float angle_between(Vector3 a, Vector3 b);

}  // namespace s3::physics::board_ground
