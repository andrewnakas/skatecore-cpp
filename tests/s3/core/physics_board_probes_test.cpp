// Port of the inline tests in skate-core/src/physics/board_probes.rs.
#include "harness.hpp"
#include "s3/core/physics/board_probes.hpp"

namespace {
using namespace s3::physics::board_probes;

S3_TEST(wall_probe_gates_and_miss_retention_follow_source) {
    WallLineInput input{100, Vector3{1.0f, 0.0f, 0.0f}, Vector3{1.0f, 0.0f, 0.0f}, Vector3{2.0f, 3.0f, 4.0f}, 0.02f};
    const WheelLine line = s3::rs::unwrap(wall_probe(input));
    CHECK_EQ(line.start, (Vector3{2.05f, 3.0f, 4.0f}));
    CHECK_EQ(line.end, (Vector3{2.05f, -1.0f, 4.0f}));
    input.state = 200;
    CHECK(!wall_probe(input).has_value());
    BoardProbeState output = BoardProbeState::default_();
    output.start(line.start);
    output.publish(BoardProbeHit{line.end, Vector3{0.0f, 1.0f, 0.0f}, 7});
    output.start(line.start);
    output.publish(std::nullopt);
    CHECK(!output.hit);
    CHECK_EQ(output.point, line.end);
    CHECK_EQ(output.surface_tag, 7u);
    output.disable();
    CHECK_EQ(output, BoardProbeState::default_());
}
}  // namespace
