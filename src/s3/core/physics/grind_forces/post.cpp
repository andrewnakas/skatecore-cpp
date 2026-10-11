// Port of skate-core/src/physics/grind_forces/post.rs.
#include "s3/core/physics/grind_forces/post.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::physics::grind_forces::post {

using board_motion_output::inverse_length_squared;
using player::wipeout::regional_force;

void check(player::wipeout::Requests& state, Settings settings, const player::wipeout::Frame& frame) {
    state.mode = 5;  // Preserve balance, timers and existing requests.
    if (dot3(frame.pose_error, frame.pose_error) >
        settings.max_displacement_208 * settings.max_displacement_208) {
        state.request(1, 0.0f);
    } else if (regional_force(frame, settings.max_body_contact_168, settings.max_arm_contact_164)) {
        state.request(0, 0.0f);
    }
    // 82D90AB8. Closing-velocity checks still run after displacement/contact.
    if (frame.board_contact) {
        const auto v = frame.closing_velocity;
        const auto& m = frame.world_to_animation;
        V local{};
        for (std::size_t i = 0; i < 4; ++i) {
            local[i] = rs::mul_add(m[2][i], v[2], rs::mul_add(m[1][i], v[1], m[0][i] * v[0]));
        }
        const V planar{local[0], 0.0f, local[2], local[3]};
        const float square = dot3(planar, planar);
        const float length_ = square == 0.0f ? 0.0f : square * inverse_length_squared(square, 2);
        if (length_ > settings.xz_acceleration_204 || rs::abs(local[1]) > 100.0f) {
            state.request(2, 0.0f);
            if (regional_force(frame, 1.0f, 20.0f)) {
                state.request(0, 0.0f);
            }
        }
    }
    // 82D909B0 always checks all three corresponding deck/Input axes.
    const float cosine = rs::min(rs::min(dot3(frame.deck[0], frame.input_board[0]),
                                         dot3(frame.deck[1], frame.input_board[1])),
                                 dot3(frame.deck[2], frame.input_board[2]));
    if (trigonometry::acos(rs::min(rs::max(cosine, -1.0f), 1.0f)) > settings.max_angular_deck_error_212) {
        state.request(3, 0.0f);
    }
}

}  // namespace s3::physics::grind_forces::post
