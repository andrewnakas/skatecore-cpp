// Port of skate-game/src/physics/grind/post.rs.
#include "s3/game/physics/grind/post.hpp"

namespace s3::game::physics::grind::post_ {

Status check(::s3::player::wipeout::requests::Requests& state, ::s3::physics::grind_forces::post::Settings settings,
             const wipeout::observations::Observations& observations) {
    S3G_TRY(const auto frame, observations.frame());
    ::s3::physics::grind_forces::post::check(state, settings, frame);
    return ok();
}

}  // namespace s3::game::physics::grind::post_
