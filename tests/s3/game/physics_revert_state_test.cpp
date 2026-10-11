// Port of skate-game/src/physics/revert_state.rs `mod tests`.
#include "harness.hpp"
#include "s3/game/physics/revert_state.hpp"

using namespace s3::game::physics::revert_state;

static RevertState state(float direction) {
    RevertState r{};
    r.speed.x = {0.0f, 0.1f, 0.2f, 0.3f, 1.0f, 2.0f, 3.0f, 7.0f};
    r.speed.y.fill(5.0f);
    r.tolerance.x = {0.0f, 0.2f, 0.6f, 1.0f};
    r.tolerance.y.fill(8.0f);
    r.gain = 0.06f;
    r.delay = 0.0f;
    r.duration = 1.0f;
    r.direction = direction;
    r.travel_sign = -1.0f;
    r.velocity = {0.0f, 0.0f, 6.0f, 0.0f};
    r.elapsed = 0.0f;
    r.captured = true;
    r.active = true;
    return r;
}

S3_TEST(revert_steers_both_directions_and_does_not_brake_faster_matching_spin) {
    for (float direction : {-1.0f, 1.0f}) {
        auto r = state(direction);
        const auto correction = r.correction({0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, V{});
        CHECK(s3::rs::abs(correction[1] - 0.3f * direction) < 0.00001f);
        CHECK(r.active);
        CHECK(r.correction({0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 6.0f * direction, 0.0f, 0.0f}) ==
              V{});
    }
}

S3_TEST(revert_completes_when_board_faces_captured_reverse_heading) {
    auto r = state(1.0f);
    r.correction({0.0f, 0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, V{});
    CHECK(!r.active);
    r.velocity = V{};
    CHECK(r.correction({0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, V{}) == V{});
}
