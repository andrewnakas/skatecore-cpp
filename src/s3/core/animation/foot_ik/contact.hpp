// Port of skate-core/src/animation/foot_ik/contact.rs.
// Foot contact history and correction82BEFC80, helpers82BF04C8/82BF0558.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"

namespace s3::animation::foot_ik::contact {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;

/// `FootContact{}` == Rust `Default`.
struct FootContact {
    /// SkeletonIK3104:0 pending,1 previous query hit,2 previous query missed.
    std::uint32_t query_state = 0;
    std::array<float, 4> position{};
    float desired_offset = 0.0f;
    float offset = 0.0f;
};

struct ContactInput {
    std::uint32_t flags_2468 = 0;
    std::size_t contact_bone = 0;
    std::array<std::size_t, 2> foot_bones{};
    std::array<float, 4> hips_world_position{};
    const Transform& inverse_board;
    const Transform& board;
    /// Actual cached line tests from Processed960/1008. Current observations
    /// replace the retained query only AFTER the previous query is consumed.
    std::array<std::optional<std::array<float, 4>>, 2> current_contacts{};
};

/// `ContactState{}` == Rust `Default`.
struct ContactState {
    std::array<FootContact, 2> feet{};
    ///3184 persists until Reset;3185 is cleared at each UpdateContact entry.
    bool support_failed = false;
    bool support_failed_this_update = false;

    void update(const std::array<status::LimbStatus, 4>& statuses, std::array<transforms::LimbFrames, 4>& frames,
                const ContactInput& input);
};

}  // namespace s3::animation::foot_ik::contact
