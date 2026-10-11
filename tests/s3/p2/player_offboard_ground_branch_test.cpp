// Port of skate-core/src/player/offboard/ground_branch/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/ground_branch.hpp"

namespace {
using namespace s3::player::offboard::ground_branch;
using s3::player::offboard::position_output::FrameOutput;
const float DT = s3::rs::from_bits(0x3c888889u);

BranchInput input() {
    return BranchInput{{0.0f, 5.0f, 0.0f, 0.0f}, {0.0f, 6.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, Vector{},
                       {0.0f, 0.0f, 1.0f, 0.0f}, 2, false};
}

S3_TEST(both_velocities_must_clear_projected_threshold) {
    CHECK(select_alternate(input()));
    auto i = input();
    i.output_velocity_512[1] = 4.0f;
    CHECK(!select_alternate(i));
    i = input();
    i.movement_velocity_480[1] = 4.0f;
    CHECK(!select_alternate(i));
}

S3_TEST(contact_distance_suppression_and_contact_bit_gate_branch) {
    auto i = input();
    i.contact_point_96 = {0.0f, 0.0f, 0.04f, 0.0f};
    CHECK(!select_alternate(i));
    i = input();
    i.suppressed_317 = true;
    CHECK(!select_alternate(i));
    i = input();
    i.contact_flags_176 = 1;
    CHECK(!select_alternate(i));
    i = input();
    i.contact_point_96 = {1.0f, 0.0f, 0.0f, 0.0f};
    CHECK(!select_alternate(i));
}

S3_TEST(reversed_contact_direction_reverses_velocity_requirement) {
    auto i = input();
    i.contact_point_96[2] = -1.0f;
    CHECK(!select_alternate(i));
    i = input();
    i.contact_point_96[2] = -1.0f;
    i.movement_velocity_480[1] = -5.0f;
    i.output_velocity_512[1] = -6.0f;
    CHECK(select_alternate(i));
}

S3_TEST(alternate_integrates_gravity_before_position_and_frame_output) {
    Vector velocity{1.0f, 0.0f, 0.0f, 0.0f};
    float speed = 0.0f;
    Vector position{};
    const Vector right{1.0f, 0.0f, 0.0f, 0.0f};
    const Vector up{0.0f, 1.0f, 0.0f, 0.0f};
    const Vector forward{0.0f, 0.0f, 1.0f, 0.0f};
    FrameOutput output{{{right, up, forward, position}}, Vector{}};
    integrate_alternate(velocity, speed, position, output, forward, up, up);
    CHECK_EQ(velocity[1], s3::rs::from_bits(0xc11ccccdu) * DT);
    CHECK_EQ(position[1], velocity[1] * DT);
    CHECK(speed > 1.0f);
    for (int i = 0; i < 3; ++i) CHECK(std::fabs(output.frame[3][i] - position[i]) < 1e-6f);
}
}  // namespace
