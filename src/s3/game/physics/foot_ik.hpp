// Port of skate-game/src/physics/foot_ik.rs.
//! Physical skeleton IK data binding. This is the production skeleton's IK owner,
//! using authored physics volumes and the real reparented animation targets.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/animation/foot_ik/drive.hpp"
#include "s3/core/animation/foot_ik/post_contact.hpp"
#include "s3/core/animation/foot_ik/post_physics.hpp"
#include "s3/core/animation/foot_ik/settings.hpp"
#include "s3/core/animation/foot_ik/state.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;
}

namespace s3::game::physics::foot_ik {

using Transform = sc::physics::skeleton_animation_record::AnimationPartTransform;

/// Physical observations from the real processed state and body owner. All
///matrices remain in stock simulation coordinates, before renderer conversion.
struct PhysicalInput {
    std::uint32_t state_id = 0;
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2480 = 0;
    std::size_t contact_bone = 0;
    /// Skeleton12496, produced by the current board-update branch.
    const Transform* physical_board = nullptr;
    /// Body skeleton's actual hips position, DataIn416.
    std::array<float, 4> hips_world_position{};
    /// Actual processed foot queries960/1008, not fabricated deck hits.
    std::array<std::optional<std::array<float, 4>>, 2> current_contacts{};
};

struct DriveOutput {
    std::array<Transform, 24> frames{};
};

struct FootIk {
    sc::animation::foot_ik::state::FootIkState state{};
    // Private in Rust.
    sc::animation::foot_ik::drive::Geometry geometry{};
    sc::animation::foot_ik::settings::Settings settings{};
    sc::animation::foot_ik::post_contact::SettingsPost post_settings{};
    std::array<std::size_t, 24> bone_indices{};

    static rs::Result<FootIk, std::string> load(const sd::Collections& data, const sd::AnimationFrames& animation,
                                                const animated_skeleton::AnimatedSkeleton& skeleton);

    void enable_feet(bool enabled);

    ///Skeleton Fill82BE1D98..1E00 publishes each physical toe frame applied
    ///to the adjacent authored inverse-part translation, not its volume COM.
    std::array<std::array<float, 4>, 2> physical_toe_positions(
        const sc::physics::skeleton_body::SkeletonPhysicalRecord& record) const;

    ///Call after native post-physics records/COM/error publication and board
    ///wobble, before SkeletonOutput::publish replaces the final pose buffers.
    std::array<bool, 4> post_physics(sc::physics::skeleton_body::SkeletonBody& body,
                                     sc::animation::foot_ik::post_physics::Input input);

    rs::Result<DriveOutput, std::string> update(const animated_skeleton::AnimatedSkeleton& skeleton,
                                                       sc::animation::Slice<const Transform> globals,
                                                       PhysicalInput input);
};

///82BED418 classF2BD493E resolves to physics_skeletonik/default. XML schema
/// gives the cached offsets; each vector retains its authored fourth word.
rs::Result<sc::animation::foot_ik::settings::Settings, std::string> load_settings(const sd::Collections& data);

}  // namespace s3::game::physics::foot_ik
