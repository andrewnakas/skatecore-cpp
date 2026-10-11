// Port of skate-core/src/player/wipeout_state/contact_response.rs and its
// private submodules material10.rs / material11.rs (whole files).
// Wipeout82D3EC08's two persistent material responses, 82D91708..82D92440.
// Each correction writes the actual 23 skater bodies' linear velocities.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::wipeout_state::contact_response {

using math::V;
using physics::skeleton_body::SkeletonBody;

struct ContactResponseInput {
    /// Original Processed608 snapshot, reused by both helpers before physics.
    V com_velocity_608{};
    /// SkeletonCollision4079/1168; absence preserves the retained normal.
    std::optional<V> material10_normal;
    /// SkeletonCollision4080/1184; the helper owns its separate contact latch.
    std::optional<V> material11_normal;
    /// Original Processed224 (effective animation transform's third axis).
    V effective_axis_224{};
    /// Original Processed2824/2828, the stock Wipeout control attributes.
    std::array<float, 2> wipeout_control_2824_2828{};
};

struct ContactResponseOutput {
    std::uint32_t material10_phase = 0;
    /// One-update pulse at first helper2->3, copied to Wipeout572.
    bool material10_finished_77 = false;
    /// Second helper's retained byte73; selects caller's collision mode9.
    bool material11_active_73 = false;
};
S3_DERIVE_EQ(ContactResponseOutput, material10_phase, material10_finished_77, material11_active_73)

// pub(super) helpers shared by the two material modules.
/// 82E0A4E8: reject the component along the safely normalized direction.
V reject(V value, V direction);
/// 82C1E170 differs: positive component test uses the normalized direction,
/// but its subtraction multiplies the ORIGINAL direction, even if nonunit.
V reject_positive(V value, V direction);
void apply_velocity_delta(SkeletonBody& body, V delta);

namespace material10 {
/// First helper's Reset91708, Select917A0, Update91908 and response kernels.
struct Response {
    V previous_velocity{};                 // 16
    V normal{0.0f, 1.0f, 0.0f, 0.0f};      // 32
    V target_velocity{};                   // 48
    float time = 0.0f;                     // 64
    std::uint32_t phase = 0;               // 72
    bool finished = false;                 // 77, cleared by caller before Select

    static Response new_() { return Response{}; }
    void reset() { *this = new_(); }
    void update(SkeletonBody& body, V velocity, std::optional<V> contact);

private:
    void select(bool contact);
};
}  // namespace material10

namespace material11 {
/// Second helper's Reset91BF8, Update91C98, response91DF8 and controls92160.
struct Response {
    V normal{0.0f, 1.0f, 0.0f, 0.0f};      // 16
    V previous_velocity{};                 // 32
    V velocity{};                          // 48
    std::int32_t frames_since_contact = 100;  // 64
    std::int32_t active_frames = 0;        // 68
    bool contact_latched = false;          // 72, reset-owned, not cleared each update
    bool active = false;                   // 73, explicitly preserved by native Reset

    /// The original constructors/reset omit initialization of byte73. Host
    /// storage begins false; subsequent native resets preserve its value.
    static Response new_() { return Response{}; }
    void reset() {
        const bool a = active;
        *this = new_();
        active = a;
    }
    void update(SkeletonBody& body, const ContactResponseInput& input);

private:
    void apply_response(SkeletonBody& body) const;
    void apply_control(SkeletonBody& body, V effective_axis, std::array<float, 2> control) const;
};
}  // namespace material11

class ContactResponse {
public:
    static ContactResponse new_() { return ContactResponse{}; }
    /// Wipeout Reset82D3B3A8 invokes both resets; byte73 survives the second.
    void reset() {
        material10_.reset();
        material11_.reset();
    }
    ContactResponseOutput update(SkeletonBody& body, const ContactResponseInput& input);

private:
    material10::Response material10_ = material10::Response::new_();
    material11::Response material11_ = material11::Response::new_();
};

}  // namespace s3::player::wipeout_state::contact_response
