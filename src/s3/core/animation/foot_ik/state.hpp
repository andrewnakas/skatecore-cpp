// Port of skate-core/src/animation/foot_ik/state.rs.
// Persistent SkeletonIK state and the GeneralUpdate82BDCA38 stage order.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "s3/core/animation/foot_ik/contact.hpp"
#include "s3/core/animation/foot_ik/drive.hpp"
#include "s3/core/animation/foot_ik/external.hpp"
#include "s3/core/animation/foot_ik/settings.hpp"
#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"

namespace s3::animation::foot_ik::state {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;
using contact::ContactState;
using drive::Geometry;
using external::ExternalTarget;
using settings::Settings;
using status::LimbStatus;
using transforms::LimbBinding;
using transforms::LimbFrames;

///82BED688 binds the toe targets to foot/toe volumes and the hand targets to
///their hand volumes. The optional second volume is distinct from chain parents.
inline constexpr std::array<LimbBinding, 4> LIMBS = {
    LimbBinding{15, std::optional<std::size_t>(16)},
    LimbBinding{19, std::optional<std::size_t>(20)},
    LimbBinding{3, std::nullopt},
    LimbBinding{7, std::nullopt},
};

struct UpdateInput {
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2480 = 0;
    /// DataIn32/96; both are actual Skeleton root transforms.
    const Transform& animation_to_world;
    const Transform& world_to_animation;
    /// DataIn160, from Skeleton12496. This is independently produced from
    ///DataIn352 (Skeleton12304) used by foot contact correction.
    const Transform& physical_board;
    const Transform& contact_board;
    const Transform& inverse_contact_board;
    /// SkeletonData animation record: physical volume frames after board offset.
    const std::array<Transform, 24>& animation;
    /// ProcessData14160: original selected globals, before volume composition.
    const std::array<Transform, 24>& original_animation;
    /// Actual board-parented targets after their type5 volume composition.
    const std::array<Transform, 4>& targets;
    std::array<float, 4> hips_world_position{};
    std::size_t contact_bone = 0;
    std::array<std::size_t, 2> foot_bones{};
    std::array<std::optional<std::array<float, 4>>, 2> current_contacts{};
};

/// Manual Rust `Default`: `FootIkState{}` == `FootIkState::default_()`.
struct FootIkState {
    //82BED780 resets the retained histories and enables feet; it is not
    //the wipeout-entry solve82BF1B90, which disables feet after its update.
    std::array<LimbStatus, 4> limbs{};
    std::array<LimbFrames, 4> frames{};
    std::array<ExternalTarget, 4> external_targets{};
    ContactState contacts{};
    // Private in Rust (accessor `feet_enabled()`).
    bool feet_enabled_ = true;

    static FootIkState default_() { return FootIkState{}; }

    void reset() { *this = FootIkState{}; }

    /// Ground::Enter writes (*(Skeleton+16432))+464 directly.
    void enable_feet(bool enabled) { feet_enabled_ = enabled; }
    bool feet_enabled() const { return feet_enabled_; }

    /// Ground collision handling can also set the per-update3185 flag.
    void mark_support_failed_this_update() { contacts.support_failed_this_update = true; }

    /// The mutable output is Skeleton12624 consumed by SkeletonDrives. The
    ///original animation and collision-volume record must remain separate.
    std::array<bool, 4> update(const UpdateInput& input, const Geometry& geometry, const Settings& settings,
                               std::array<Transform, 24>& drives);
};

}  // namespace s3::animation::foot_ik::state
