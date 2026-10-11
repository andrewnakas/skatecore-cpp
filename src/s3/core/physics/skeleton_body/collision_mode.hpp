// Port of skate-core/src/physics/skeleton_body/collision_mode.rs (included via skeleton_body.hpp).
// Original normal body setup82BE7280 and driven selector82D913C0 modes1/5/6.
// Collision volume groups and part metadata are distinct from body state bits.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/contact.hpp"

namespace s3::physics::skeleton_body::collision_mode {

using contact::RetailContactMaterial;

struct SkeletonCollisionSettings {
    bool enabled = false;
    RetailContactMaterial normal_material;
    /// Original collision classification settings copied by82BE745C/7468.
    std::array<bool, ANIMATION_PART_COUNT> compliant{};
    std::array<float, ANIMATION_PART_COUNT> priority{};
    float effect_time = 0.0f;
};

struct SkeletonPartCollision {
    /// Volume+92 bit0. Other volume flags are owned by geometry construction.
    bool enabled = false;
    /// Volume+84; group4 is written by DisableCollisionWithWorld.
    std::uint32_t volume_group = 0;
    /// Original Part+92, distinct from its volume group.
    std::uint32_t part_group = 0;
    RetailContactMaterial material;
};

class SkeletonCollisionMode {
public:
    std::array<SkeletonPartCollision, PART_COUNT> parts{};
    std::array<std::uint32_t, ANIMATION_PART_COUNT> disable_count{};
    /// Original Body8224 gates postphysics countdown/reenabling.
    bool pending_reenable = false;
    std::uint32_t assembly_group = 0;
    bool partial_ragdoll = false;
    bool is_ragdoll = false;
    SkeletonCollisionSettings settings;
    ///82BE82C0 table: true suppresses this pair before geometric collision.
    std::array<std::array<bool, PART_COUNT>, PART_COUNT> self_culling{};

    ///82D91298: Body volume pointers start at3092, counters at8128.
    /// The stores at3120/3104/3124/3108/3096 therefore select parts7/3/8/4/1:
    /// both hands, both forearms and the head, not the next bone in each chain.
    void disable_handplant_contacts(std::uint32_t frames);

    ///82BE7078, used by the real teleport path82BE3508. Collision volume
    ///enabled flags and materials survive this reset.
    void reset_body_state(SkeletonCollisionFeedback& feedback);

    /// Full normal constructor state after82BE4290 and SetUpNormal. The final
    /// argument is the actual constructor byte8227, not a guessed AI identity.
    static SkeletonCollisionMode new_normal(SkeletonCollisionSettings settings, bool cull_all_self_pairs);

    ///Normal riding and ascending-Air collision. Mode6 calls82BE3700, whose
    ///root/extra disabling, group5 and eligible-bone writes match mode5's result.
    rs::Result<rs::Unit, const char*> select_driven(std::uint32_t mode);

    ///82BE70D8. Does not consult BoneHasCollision, clear positive disable
    ///counts, alter materials or disable the two extra volumes.
    void normal_collision();

    ///82BE6FF8. Bone volume group values survive this operation.
    void disable_all(bool clear_counts);

    ///82BE7190. This operation tests the counter but does not decrement it.
    void enable_bone(std::size_t part);

    ///82BD8364..83BC runs after collision observations and before82BD9FB0.
    /// It only changes the enabled bit, preserving each volume's group.
    void finish_contact_frame();

    ///82BE71F0, unlike the whole-body normal path, uses BoneHasCollision.
    void normal_bone(std::size_t part, bool has_collision);

    /// Physical-property portion of82BE7280. The coordinator separately resets
    /// the real SkeletonCollision observations and reinstalls normal joint
    /// limits in that method's original order. This method does not fake either.
    void restore_normal_properties(SkeletonBody& skeleton);

    ///Physical properties of82BE6D60; joint limits are set first by its caller.
    void apply_ragdoll_properties(SkeletonBody& skeleton, bool inverse_mass, bool inverse_inertia,
                                  std::array<float, 2> drag, std::array<RetailContactMaterial, 2> materials);

    ///The post-body portions of original selector modes7..10.
    void finish_ragdoll_request(std::uint32_t mode);

private:
    std::array<std::array<bool, PART_COUNT>, PART_COUNT> normal_self_culling{};

    void set_group(std::uint32_t group);
    ///Request4 branch82D914B0 and Body82BE5248. Existing physical masses,
    ///joint limits, material and self-pair table survive this policy change.
    void select_biped();
    void disable_root();
    void enable_eligible_bones();
};

}  // namespace s3::physics::skeleton_body::collision_mode

namespace s3::physics::skeleton_body {
using collision_mode::SkeletonCollisionMode;
using collision_mode::SkeletonCollisionSettings;
using collision_mode::SkeletonPartCollision;
}  // namespace s3::physics::skeleton_body
