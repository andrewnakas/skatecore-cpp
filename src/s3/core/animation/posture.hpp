// Port of skate-core/src/animation/posture.rs.
//! TU3 posture profile selection and the pending GetAnimTree construction latch.
//! 82590B50 maps profile values; 82678990 sets the latch; 82B984C8 consumes it.
//! Channel construction bypasses this service entirely (82D1CC20/82D1D0F0).
#pragma once
#include <cstdint>
#include <optional>
#include <utility>

#include "s3/core/rs.hpp"

namespace s3::animation::posture {

/// Registration order at 82858810, independent of asset directory ordering.
enum class PosturePose {
    Stiff,
    Slouch,
    Buff,
};

/// `PosturePose::from_profile`.
constexpr std::optional<PosturePose> posture_pose_from_profile(std::uint32_t value) {
    switch (value) {
        case 1: return PosturePose::Stiff;
        case 2: return PosturePose::Slouch;
        case 3: return PosturePose::Buff;
        default: return std::nullopt;
    }
}

/// `PosturePose::name`.
constexpr const char* posture_pose_name(PosturePose pose) {
    switch (pose) {
        case PosturePose::Stiff: return "POSTURE_STIFF_POSE";
        case PosturePose::Slouch: return "POSTURE_SLOUCH_POSE";
        case PosturePose::Buff: return "POSTURE_BUFF_POSE";
    }
    return "";
}

/// Actual profile selection and a request for the next eligible main tree.
/// Native reset 824FA9A0 supplies profile 0; constructor 82B973C8 clears pending.
class PendingPosture {
public:
    /// Profile refresh changes selection without inventing a playback request.
    void set_profile(std::uint32_t profile) { profile_ = profile; }

    std::uint32_t profile() const { return profile_; }

    /// Every Play Begin overwrites this latch, including an authored false.
    void set_requested(bool requested) { pending_ = requested; }

    bool is_pending() const { return pending_; }

    std::optional<PosturePose> selected_pose() const {
        if (pending_) {
            return posture_pose_from_profile(profile_);
        }
        return std::nullopt;
    }

    /// Wrap a successfully built main motion tree before its bind-pose wrapper.
    /// The caller must first establish a valid registered static-pose bank.
    /// Missing motion/bank must skip this call and leave the latch untouched.
    /// Clear only after posture construction succeeds, before outer bind work.
    /// `wrap` is `FnOnce(T, PosturePose) -> rs::Result<T, E>`.
    template <class T, class E, class F>
    rs::Result<T, E> apply(T motion, F&& wrap) {
        const std::optional<PosturePose> pose = selected_pose();
        if (!pose) {
            return rs::Result<T, E>(std::move(motion));
        }
        rs::Result<T, E> wrapped = wrap(std::move(motion), *pose);
        if (wrapped.is_err()) return wrapped;
        pending_ = false;
        return wrapped;
    }

    bool operator==(const PendingPosture& o) const { return profile_ == o.profile_ && pending_ == o.pending_; }
    bool operator!=(const PendingPosture& o) const { return !(*this == o); }

private:
    std::uint32_t profile_ = 0;
    bool pending_ = false;
};

}  // namespace s3::animation::posture
