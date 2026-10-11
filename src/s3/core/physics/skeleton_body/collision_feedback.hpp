// Port of skate-core/src/physics/skeleton_body/collision_feedback.rs (included via skeleton_body.hpp).
// SkeletonCollision observations from original TU3 82BD4A30/82BD5DA0.
// Reports must come from the completed solver, in native report order.
//
// The Rust `impl SkeletonCollisionFeedback` blocks in collision_filter.rs and
// collision_update.rs are declared here and defined in their own .cpp files.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::collision_feedback {

using skeleton_animation_record::AnimationPartTransform;
using collision_mode::SkeletonCollisionSettings;

/// pub(super) type V = [f32; 4].
using V = std::array<float, 4>;

struct SkeletonFeedbackSettings {
    SkeletonCollisionSettings body;
    float small_object_mass = 0.0f;
    float ground_plane_max_distance = 0.0f;
    float ground_plane_max_angle = 0.0f;
    float skater_scalar = 0.0f;
    float ai_scalar = 0.0f;
    V groin_offset{};
    V face_offset{};
    float groin_radius = 0.0f;
    float face_radius = 0.0f;
};

struct SkeletonContactBody {
    std::uint32_t state_flags = 0;
    float inverse_mass = 0.0f;
    V linear_velocity{};
};

/// The useful fields of original 96-byte ContactInfo and its solved spy.
/// `normal` already has the report-side sign; `solved_vector` is spy+80.
struct SkeletonContactReport {
    std::size_t part = 0;
    V normal{};
    V point{};
    std::uint32_t tag = 0;
    std::uint32_t other_group = 0;
    std::optional<std::int32_t> other_entity;
    SkeletonContactBody body_a;
    SkeletonContactBody body_b;
    bool side_a = false;
    V solved_vector{};
};

/// Original UpdatePostPhysics82BD80D8 builds these values before Update.
/// (Rust borrows `'a` references; held here as const references.)
struct SkeletonCollisionInput {
    float dt;
    V plane_point;
    V plane_normal;
    V reference_velocity;
    V com_velocity;
    bool ragdoll;
    /// Processed2472 bit0x8000, which suppresses normal ground filtering.
    bool disable_ground_filter;
    /// Processed2472 bit0x10000000 selects AISkeletonScalar.
    bool ai_collision_scalar;
    /// Category500, excluding states501 and503.
    bool offboard;
    bool entering_offboard;
    bool category_600;
    bool request_partial_ragdoll;
    const SkeletonPhysicalRecord& physical;
    const std::array<float, 24>& part_weights;
    /// Actual GetPartTransform frames used by CheckConflictingContacts.
    const std::array<AnimationPartTransform, 26>& body_frames;
};

struct BoneContact {
    V normal{};
    V specific_normal{};
    V tangent{};
    V specific_tangent{};
    V point{};
    float force = 0.0f;
    float specific_force = 0.0f;
    std::uint32_t tag = 0;
    std::uint32_t specific_tag = 0;
    std::array<bool, 4> groups{};
};

struct ContactRegion {
    float force = 0.0f;
    float weighted_force = 0.0f;
    float tangent_speed = 0.0f;
    std::uint32_t material_flags = 0;
    V normal{};
    std::optional<std::size_t> part;
};

struct ContactPlane {
    V normal{};
    std::size_t part = 0;
};

struct SpecificContact {
    std::size_t part = 0;
    V local_point{};
    V world_point{};
    float radius_squared = 0.0f;
    bool current = false;
    bool recent = false;
};

struct SkeletonContactFlags {
    bool material_6 = false;
    bool noncompliant = false;
    /// Native4070.
    bool compliant = false;
    bool recovering = false;
    bool group_8 = false;
    bool nonboard = false;
    bool ragdoll = false;
    bool conflicting = false;
    bool impaled = false;
    /// Native4077. This does not itself contain the pose-error vector16272.
    bool has_impulse = false;
    bool foot_board = false;
    bool material_10 = false;
    bool material_11 = false;
    bool material_12 = false;
    bool any = false;
};

struct SkeletonCollisionFeedback {
    SkeletonFeedbackSettings settings;
    std::array<float, 24> contact_age{};
    std::array<float, 24> priority{};
    std::array<bool, 24> compliant{};
    std::array<bool, 24> current{};
    std::array<BoneContact, 24> bones{};
    std::array<ContactRegion, 8> regions{};
    std::vector<ContactPlane> planes;
    std::array<SpecificContact, 2> specific{};
    V highest_normal{};
    V foot_normal{};
    std::array<V, 2> material_normals{};
    float timer = 0.0f;
    /// Native4028. Reset is zero; Update recovers toward one.
    float drive_weight = 0.0f;
    float maximum_priority = 0.0f;
    std::array<float, 3> wipeout_times{};
    float maximum_skater_force = 0.0f;
    std::int32_t other_skater = 0;
    float maximum_group_8_force = 0.0f;
    float maximum_group_11_force = 0.0f;
    float material_12_height = 0.0f;
    SkeletonContactFlags flags;

    /// Constructor82BD5F48/6038 plus the final Body constructor Reset. Static
    /// compliant settings survive Reset, while the priority array is cleared.
    static SkeletonCollisionFeedback new_(SkeletonFeedbackSettings settings);

    ///82BD5DA0 preserves compliant settings, specific-point histories and4082.
    void reset();

    /// SetUpNormal82BE7280 calls Reset before reinstalling authored priority
    /// and compliant settings. A subsequent constructor Reset clears priority.
    void set_up_normal();

    // ---- collision_filter.rs ----
    V filter_error(V error, V axis) const;

    // ---- collision_update.rs ----
    void update(const SkeletonCollisionInput& input, const std::vector<SkeletonContactReport>& reports);
    void update(const SkeletonCollisionInput& input, const SkeletonContactReport* reports,
                std::size_t reports_len);

private:
    void begin_frame(const SkeletonCollisionInput& input);
    bool ignore_ground(const SkeletonCollisionInput& input, const SkeletonContactReport& report) const;
    void observe_normal(bool ragdoll, const SkeletonContactReport& report, std::uint32_t material);
    void record_force(const SkeletonContactReport& report, bool specific, V relative, float force,
                      float weighted_force);
    /// Original82BD5770 calls GetPartTransform, not animation-pose lookup.
    bool check_conflicting(const SkeletonCollisionInput& input) const;
    bool check_impaled() const;
};

}  // namespace s3::physics::skeleton_body::collision_feedback

namespace s3::physics::skeleton_body {
using collision_feedback::BoneContact;
using collision_feedback::ContactPlane;
using collision_feedback::ContactRegion;
using collision_feedback::SkeletonCollisionFeedback;
using collision_feedback::SkeletonCollisionInput;
using collision_feedback::SkeletonContactBody;
using collision_feedback::SkeletonContactFlags;
using collision_feedback::SkeletonContactReport;
using collision_feedback::SkeletonFeedbackSettings;
using collision_feedback::SpecificContact;
}  // namespace s3::physics::skeleton_body
