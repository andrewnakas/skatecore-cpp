// Port of skate-game/src/physics/biped_ground/entry/tests.rs
#include "harness.hpp"
#include "s3/game/physics/biped_ground/entry.hpp"

using s3::game::physics::biped_ground::entry::effective_root;
using Frame = s3::game::physics::biped_ground::entry::Frame;

namespace {
const Frame ROOT{{{1.0f, 2.0f, 3.0f, 4.0f}, {5.0f, 6.0f, 7.0f, 8.0f}, {9.0f, 10.0f, 11.0f, 12.0f},
                  {13.0f, 14.0f, 15.0f, 16.0f}}};
}

S3_TEST(ordinary_entry_preserves_the_complete_root) {
    CHECK(effective_root(ROOT, 0) == ROOT);
    CHECK(effective_root(ROOT, ~4u) == ROOT);
}

S3_TEST(mirrored_entry_flips_only_x_and_z_including_fourth_lanes) {
    const auto result = effective_root(ROOT, 4);
    CHECK((result[0] == std::array<float, 4>{-1.0f, -2.0f, -3.0f, -4.0f}));
    CHECK((result[2] == std::array<float, 4>{-9.0f, -10.0f, -11.0f, -12.0f}));
    CHECK(result[1] == ROOT[1]);
    CHECK(result[3] == ROOT[3]);
}
