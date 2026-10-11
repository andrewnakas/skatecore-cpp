// Port of skate-game/src/physics/wipeout_states/contact.rs
//! Wipeout300's material corrections and collision-policy requests82D3EC08.
#include "s3/game/physics/wipeout_states/contact.hpp"

namespace s3::game::physics::wipeout_states::contact {

using ::s3::player::wipeout_state::contact_response::ContactResponseInput;

Status update(::s3::player::wipeout_state::State& state,
              ::s3::player::wipeout_state::contact_response::ContactResponse& response,
              ::s3::physics::skeleton_body::SkeletonBody& body,
              const ::s3::physics::skeleton_body::SkeletonCollisionFeedback& feedback,
              skeleton_controller::SkeletonControllerState& controller,
              ::s3::physics::skeleton_body::SkeletonCollisionMode& collision,
              ::s3::physics::skeleton_body::SkeletonJoints& joints, const ragdoll::RagdollSetup& ragdoll,
              bool contact, std::array<float, 4> com_velocity, std::array<float, 4> effective_axis,
              std::array<float, 2> controls) {
    ContactResponseInput input{};
    input.com_velocity_608 = com_velocity;
    if (feedback.flags.material_10) input.material10_normal = feedback.material_normals[0];
    if (feedback.flags.material_11) input.material11_normal = feedback.material_normals[1];
    input.effective_axis_224 = effective_axis;
    input.wipeout_control_2824_2828 = controls;
    const auto output = response.update(body, input);
    state.material_ten_response = output.material10_finished_77;
    // The original comparison rejects <=0.3; unordered values retain true.
    const bool air_mode = !contact && !(state.predicted_time <= 0.3f);
    const bool air_changed = state.air_collision_mode != air_mode;
    state.air_collision_mode = air_mode;
    const bool material_changed = state.material_eleven_response != output.material11_active_73;
    state.material_eleven_response = output.material11_active_73;
    if (!state.special_surface && (air_changed || material_changed)) {
        const std::uint32_t requested = state.material_eleven_response ? 9u : (air_mode ? 11u : 8u);
        S3G_CHECK(ragdoll.request(controller, requested, body, joints, collision));
    }
    return ok();
}

}  // namespace s3::game::physics::wipeout_states::contact
