// Port of skate-core/src/player/offboard/position_output/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/position_output.hpp"

namespace {
using namespace s3::player::offboard::position_output;
const Vector UP{0.0f, 1.0f, 0.0f, 0.0f};
const Vector FORWARD{0.0f, 0.0f, 1.0f, 0.0f};
void close(float a, float b) { CHECK_MSG(std::fabs(a - b) < 2e-6f, a << " != " << b); }
FrameOutput frame() { return FrameOutput{{{{1.0f, 0.0f, 0.0f, 0.0f}, UP, FORWARD, Vector{}}}, Vector{}}; }
PositionInput input() {
    PositionInput i;
    i.projection_axis_416 = UP;
    i.animation_position_272 = {2.0f, 0.0f, 3.0f, 0.0f};
    return i;
}

S3_TEST(frame_tracks_horizontal_motion_but_limits_downward_velocity_change) {
    auto state = frame();
    state.update(FORWARD, UP, UP, {1.0f, -1.0f, 2.0f, 0.0f});
    close(state.velocity[0], 60.0f);
    close(state.velocity[1], -1.0f);
    close(state.velocity[2], 120.0f);
    close(state.frame[3][0], 1.0f);
    close(state.frame[3][1], -1.0f / 60.0f);
    state.update(FORWARD, UP, UP, {1.0f, -1.0f, 2.0f, 0.0f});
    close(state.velocity[1], -2.0f);
    close(state.frame[3][1], -3.0f / 60.0f);
}

S3_TEST(upward_frame_velocity_is_not_limited_and_basis_is_normalized) {
    auto state = frame();
    state.update({0.0f, 0.0f, 3.0f, 0.0f}, UP, UP, {0.0f, 1.0f, 0.0f, 0.0f});
    close(state.velocity[1], 60.0f);
    close(state.frame[3][1], 1.0f);
    close(state.frame[0][0], 1.0f);
    close(state.frame[2][2], 1.0f);
}

S3_TEST(position_follows_planar_animation_and_limits_vertical_change_per_update) {
    Vector position{};
    update_position(position, input());
    CHECK_EQ(position[0], 2.0f);
    CHECK_EQ(position[2], 3.0f);
    close(position[1], 0.1f);
    update_position(position, input());
    close(position[1], 0.2f);
}

S3_TEST(alternate_origin_precedes_first_contact_and_second_contact_is_capped) {
    Vector position{0.0f, 0.95f, 0.0f, 0.0f};
    auto i = input();
    i.contact_flags_176 = 3;
    i.contact_origin_0 = {0.0f, 10.0f, 0.0f, 0.0f};
    i.contact_origin_96 = {0.0f, 20.0f, 0.0f, 0.0f};
    i.use_override_711 = true;
    i.override_origin_592 = {0.0f, -0.3f, 0.0f, 0.0f};
    update_position(position, i);
    close(position[1], 0.95f);  // override-.3 then second-contact+.3 then offset+.95.
}
}  // namespace
