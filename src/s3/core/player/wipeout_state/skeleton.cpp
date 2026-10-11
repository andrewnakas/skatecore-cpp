// Port of skate-core/src/player/wipeout_state/skeleton.rs (whole file).
#include "s3/core/player/wipeout_state/skeleton.hpp"

namespace s3::player::wipeout_state::skeleton {

using physics::skeleton_animation_record::compose_affine;
using physics::skeleton_root::inverse_rigid;

void update_roots(physics::skeleton_root::SkeletonRootFrames& roots,
                  const physics::skeleton_animation_record::AnimationPartTransform& animation_hips,
                  const physics::skeleton_animation_record::AnimationPartTransform& physical_hips,
                  std::array<float, 4> hips_velocity, std::array<float, 4> physical_com,
                  std::array<float, 4> com_velocity, float state_time) {
    const float DT = rs::from_bits(0x3C888889u);
    const auto inverse_hips = inverse_rigid(animation_hips);
    auto predicted_hips = physical_hips;
    predicted_hips[3] =
        rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(hips_velocity[i], DT, physical_hips[3][i]); });
    auto root = compose_affine(predicted_hips, inverse_hips);
    float weight = state_time + DT;
    weight = -weight >= 0.0f ? 0.0f : weight;
    weight = 1.0f - weight >= 0.0f ? weight : 1.0f;
    auto desired = physical_com;
    desired[1] += rs::from_bits(0xBF4CCCCDu);
    root[3] = rs::from_fn<4>([&](std::size_t i) {
        const float predicted = rs::mul_add(com_velocity[i], DT, root[3][i]);
        return rs::mul_add(desired[i], weight, predicted * (1.0f - weight));
    });
    roots.animation_to_world = root;
    roots.world_to_animation = inverse_rigid(root);
}

}  // namespace s3::player::wipeout_state::skeleton
