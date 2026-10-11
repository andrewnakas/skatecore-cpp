// Port of skate-core/src/physics/skeleton_body/tests/collision_feedback.rs.
#include "harness.hpp"
#include "s3/core/physics/skeleton_body.hpp"

namespace {
using namespace s3::physics::skeleton_body;
using s3::physics::contact::RetailContactMaterial;
using s3::physics::skeleton_animation_record::AnimationPartTransform;
using s3::physics::skeleton_animation_record::IDENTITY;
using V = std::array<float, 4>;


SkeletonFeedbackSettings settings() {
    SkeletonFeedbackSettings s;
    s.body.enabled = true;
    s.body.normal_material = RetailContactMaterial{0.0f, 0.0f, 0.0f};
    s.body.compliant.fill(true);
    s.body.priority.fill(0.0f);
    s.body.effect_time = 0.25f;
    s.small_object_mass = 5.5f;
    s.ground_plane_max_distance = 0.3f;
    s.ground_plane_max_angle = 0.8f;
    s.skater_scalar = 0.5f;
    s.ai_scalar = 1.0f;
    s.groin_offset = {0.0f, 0.0f, 0.0f, 0.0f};
    s.face_offset = {0.0f, 0.0f, 0.0f, 0.0f};
    s.groin_radius = 0.0f;
    s.face_radius = 0.0f;
    return s;
}

S3_TEST(handplant_contact_mask_survives_updates_and_restores_on_exit) {
    SkeletonCollisionMode collision = SkeletonCollisionMode::new_normal(settings().body, false);
    for (int i = 0; i < 3; ++i) {
        collision.disable_handplant_contacts(2);
        // A driven-state refresh must not restore these volumes mid-plant.
        collision.select_driven(5).unwrap();
        for (std::size_t part : {1, 3, 4, 7, 8}) {
            CHECK_MSG(!collision.parts[part].enabled, "plant contact part " << part);
        }
        for (std::size_t part : {2, 5, 6, 9, 10, 15, 19}) {
            CHECK_MSG(collision.parts[part].enabled, "unaffected part " << part);
        }
        collision.finish_contact_frame();
        CHECK(collision.pending_reenable);
        CHECK_EQ(collision.disable_count[3], 1u);
        CHECK(!collision.parts[3].enabled);
    }
    // No refresh from the plant on the next tick: restore normal contact.
    collision.finish_contact_frame();
    CHECK(!collision.pending_reenable);
    for (std::size_t part : {1, 3, 4, 7, 8}) {
        CHECK(collision.parts[part].enabled);
        CHECK_EQ(collision.disable_count[part], 0u);
    }
}

SkeletonCollisionInput input(const SkeletonPhysicalRecord& physical, const std::array<AnimationPartTransform, 26>& frames,
                             const std::array<float, 24>& weights) {
    return SkeletonCollisionInput{
        0.125f,
        V{0.0f, 0.0f, 0.0f, 0.0f},
        V{0.0f, 1.0f, 0.0f, 0.0f},
        V{2.0f, 0.0f, 0.0f, 0.0f},
        V{0.0f, 0.0f, 0.0f, 0.0f},
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        physical,
        weights,
        frames,
    };
}

SkeletonContactReport report(V normal) {
    SkeletonContactReport r;
    r.part = 2;
    r.normal = normal;
    r.point = {0.0f, 0.0f, 0.0f, 0.0f};
    r.tag = 0;
    r.other_group = 0;
    r.other_entity = std::nullopt;
    r.body_a = SkeletonContactBody{4, 0.5f, V{2.0f, 0.0f, 0.0f, 0.0f}};
    r.body_b = SkeletonContactBody{1, 0.0f, V{0.0f, 0.0f, 0.0f, 0.0f}};
    r.side_a = true;
    r.solved_vector = {100.0f, 0.0f, 0.0f, 0.0f};
    return r;
}

std::array<AnimationPartTransform, 26> identity_frames() {
    std::array<AnimationPartTransform, 26> f{};
    f.fill(IDENTITY);
    return f;
}

S3_TEST(support_contact_is_filtered_but_wall_contact_changes_drives_and_recovers) {
    SkeletonPhysicalRecord physical = SkeletonPhysicalRecord::default_();
    physical.velocities[2] = {2.0f, 0.0f, 0.0f, 0.0f};
    const auto frames = identity_frames();
    std::array<float, 24> weights{};
    weights.fill(1.0f);
    const auto in = input(physical, frames, weights);
    SkeletonCollisionFeedback collision = SkeletonCollisionFeedback::new_(settings());
    collision.update(in, std::vector<SkeletonContactReport>{report({0.0f, 1.0f, 0.0f, 0.0f})});
    CHECK(collision.flags.any);
    CHECK(!collision.flags.compliant);
    CHECK_EQ(collision.drive_weight, 1.0f);
    collision.update(in, std::vector<SkeletonContactReport>{report({-1.0f, 0.0f, 0.0f, 0.0f})});
    CHECK(collision.flags.compliant && collision.flags.has_impulse);
    CHECK_EQ(collision.bones[2].force, 2.0f);
    CHECK_EQ(collision.drive_weight, 0.0f);
    CHECK(s3::rs::is_finite(collision.regions[1].weighted_force));
    collision.update(in, std::vector<SkeletonContactReport>{});
    CHECK_EQ(collision.drive_weight, 0.5f);
    collision.update(in, std::vector<SkeletonContactReport>{});
    CHECK_EQ(collision.drive_weight, 1.0f);
}

S3_TEST(tiny_dynamic_opponent_is_noncompliant_even_for_compliant_bone) {
    SkeletonPhysicalRecord physical = SkeletonPhysicalRecord::default_();
    physical.velocities[2] = {2.0f, 0.0f, 0.0f, 0.0f};
    const auto frames = identity_frames();
    std::array<float, 24> weights{};
    weights.fill(1.0f);
    const auto in = input(physical, frames, weights);
    SkeletonContactReport contact = report({-1.0f, 0.0f, 0.0f, 0.0f});
    contact.body_b.state_flags = 4;
    contact.body_b.inverse_mass = 1.0f;
    SkeletonCollisionFeedback collision = SkeletonCollisionFeedback::new_(settings());
    collision.update(in, std::vector<SkeletonContactReport>{contact});
    CHECK_EQ(collision.bones[2].force, 1.0f);
    CHECK(collision.flags.noncompliant);
    CHECK(!collision.flags.compliant && !collision.flags.has_impulse);
}

S3_TEST(contact_planes_constrain_physical_pose_error_without_inventing_impulse) {
    SkeletonCollisionFeedback collision = SkeletonCollisionFeedback::new_(settings());
    const V axis{0.0f, 1.0f, 0.0f, 0.0f};
    const V error{2.0f, 3.0f, 0.0f, 0.0f};
    CHECK_EQ(collision.filter_error(error, axis), (V{0.0f, 0.0f, 0.0f, 0.0f}));
    collision.planes.push_back(ContactPlane{V{1.0f, 0.0f, 0.0f, 0.0f}, 2});
    const V filtered = collision.filter_error(error, axis);
    CHECK(s3::rs::abs(filtered[0] - 2.0f) < 1e-5f);
    CHECK_EQ(filtered[1], 0.0f);
    const float diagonal = s3::rs::sqrt(0.5f);
    collision.planes[0].normal = {diagonal, 0.0f, diagonal, 0.0f};
    const V projected = collision.filter_error(error, axis);
    CHECK(s3::rs::abs(projected[0] - 1.0f) < 1e-5f);
    CHECK(s3::rs::abs(projected[2] - 1.0f) < 1e-5f);
    collision.planes[0].normal = {-1.0f, 0.0f, 0.0f, 0.0f};
    CHECK_EQ(collision.filter_error(error, axis), (V{0.0f, 0.0f, 0.0f, 0.0f}));
}
}  // namespace
