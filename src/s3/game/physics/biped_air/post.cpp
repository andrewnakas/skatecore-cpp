// Port of skate-game/src/physics/biped_air/post.rs
//! State501 Post82D2FFF8, after completed physical skeleton feedback.
#include "s3/game/physics/biped_air/post.hpp"

#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::biped_air::post {

Status post(SkaterRuntime& skater, Frame frame) {
    //82D30018..30: synchronize the SAME manager state503 will consume.
    S3G_CHECK(skater.landing_deck.post_physics(skater.player_input));
    const auto& p = skater.player_input.processed;
    S3G_TRY(const auto mode, skater.wipeout.mode(p));
    PostInput input{};
    input.flags_2484 = p.flags_2484;
    input.state_timer_2664 = p.state_timer_2664;
    input.forward_224 = rs::from_bits(p.effective_anim_transform_192[2]);
    input.side_192 = rs::from_bits(p.effective_anim_transform_192[0]);
    input.input_2708 = skater.animation_input.extra.look_y;
    input.input_2704 = skater.animation_input.extra.look_x;
    skater.biped_air.state.post_physics(input, skater.biped_air.checks, frame, mode, skater.skeleton_input.root_velocity,
                                        skater.wipeout.state);
    return ok();
}

}  // namespace s3::game::physics::biped_air::post
