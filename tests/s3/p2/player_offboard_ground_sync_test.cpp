// Port of skate-core/src/player/offboard/ground_sync/tests.rs.
#include <string>
#include <vector>

#include "harness.hpp"
#include "s3/core/player/offboard/ground_sync.hpp"

namespace {
using namespace s3::player::offboard::ground_sync;
namespace ground_query = s3::player::offboard::ground_query;
using s3::rs::Unit;

const Frame FRAME{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}};
Vector splat(float v) { return Vector{v, v, v, v}; }

CompletedMotion motion() {
    return CompletedMotion{{0.0f, 2.0f, 0.0f, 1.0f}, 1, FRAME, FRAME, splat(1.0f), splat(2.0f), splat(3.0f),
                           splat(0.0f), splat(4.0f), false, true};
}
Processed make_processed() { return Processed{FRAME, Vector{}, 0, 0, 0, 0, 0.0f, 0.0f, ground_query::QueryContext{0, 0}}; }

S3_TEST(explicit_jump_suppression_does_not_suppress_controller_launch) {
    State state{};
    auto m = motion();
    auto p = make_processed();
    p.flags_2476 = 0x80000;
    CHECK(wants_air(state, m, p));
    for (std::uint32_t mask : {0x20000u, 0x80u, 0x100u}) {
        p.flags_2480 = mask;
        CHECK(!wants_air(state, m, p));
        m.launch_1076 = true;
        CHECK(wants_air(state, m, p));
        m.launch_1076 = false;
    }
    p.flags_2480 = 0;
    state.flags_144_to_150[6] = true;
    CHECK(!wants_air(state, m, p));
    m.contact_flags_368 = 0;
    p.elapsed_2664 = 0.05f;
    CHECK(!wants_air(state, m, p));
    p.elapsed_2664 = s3::rs::from_bits(s3::rs::to_bits(0.05f) + 1);
    CHECK(wants_air(state, m, p));
}

S3_TEST(board_bounds_flattens_y_but_retains_scaled_w) {
    Frame frame = FRAME;
    frame[2] = {0.0f, 9.0f, 2.0f, 6.0f};
    const auto b = board_bounds(frame, {99.0f, 3.0f, 4.0f, 55.0f}, {1.0f, 2.0f, 5.0f, 8.0f});
    CHECK(std::fabs(b.frame[2][3] - 3.0f) < 1e-5f);
    CHECK(std::fabs(b.frame[3][3] - 28.0f) < 1e-4f);
    CHECK_EQ(b.frame[3][1], 3.0f);
    CHECK_EQ(b.extents, (Vector{1.0f, 2.0f, 5.0f, 8.0f}));
    frame[2] = {0.0f, 7.0f, 0.0f, 8.0f};
    CHECK_EQ(board_bounds(frame, Vector{}, Vector{}).frame[2], (Vector{0.0f, 0.0f, 1.0f, 0.0f}));
}

using Key = std::array<std::uint32_t, 2>;
struct Recorder : Services<Unit, Unit, Key> {
    Processed p = make_processed();
    std::vector<std::string> events;
    Vector point{};
    Frame bone_frame = FRAME;
    std::optional<ToolkitInput> toolkit;
    bool found = false;
    bool accepted = false;
    bool edge = false;

    s3::rs::Result<std::vector<ground_query::Edge>, Unit> edge_candidates(const ground_query::EdgeSearch& s) override {
        events.push_back("edges");
        CHECK(s.narrow_forward);
        std::vector<ground_query::Edge> out;
        if (edge) {
            out.push_back(ground_query::Edge{s3::math::Vector3::new_(-0.2f, 0.0f, 0.0f), s3::math::Vector3::new_(0.2f, 0.0f, 0.0f)});
        }
        return out;
    }
    s3::rs::Result<std::array<std::optional<ground_query::LineHit>, 7>, Unit> query_lines(
        const ground_query::GroundQueryPacket&) override {
        S3_PANIC("Sync must not consume its newly submitted queries");
    }
    Processed processed() const override { return p; }
    Unit prepare_biped_launch() override {
        events.push_back("prepare");
        point = {2.0f, 3.0f, 4.0f, 90.0f};
        return Unit{};
    }
    Vector skeleton_point_10960() const override { return point; }
    void launch_air(Unit, Vector pt) override {
        events.push_back("air");
        CHECK_EQ(pt, (Vector{2.0f, 3.0f, 4.0f, 1.0f}));
    }
    void update_skeleton(Frame frame, Vector) override {
        events.push_back("skeleton");
        bone_frame = frame;
        // Exercises the required fresh processed read after this callback.
        p.flags_2476 = 0x400000;
    }
    BoardSettings board_settings() const override {
        return BoardSettings{splat(2.0f), splat(3.0f), Vector{}, 10.0f, 20.0f, 0.2f, 30.0f, 40.0f};
    }
    std::uint8_t board_flags_12836() const override { return 0x40; }
    std::optional<Key> probe_board(Vector) override {
        events.push_back("probe");
        return found ? std::optional<Key>(Key{7, 11}) : std::nullopt;
    }
    Key candidate_key_188(const Key& c) const override { return c; }
    Vector bone_23_position() override {
        events.push_back("bone23");
        return Vector{};
    }
    bool classify_board(const Key&, Vector, Bounds b, BoardLimits l) override {
        events.push_back("classify");
        CHECK_EQ(b.extents, splat(1.9f));
        CHECK(std::fabs(l.angle_a - 30.0f * s3::rs::from_bits(0x3c8efa35u)) < 1e-6f);
        return accepted;
    }
    void query_board(Vector, Bounds b, BoardLimits l) override {
        events.push_back("board_query");
        CHECK_EQ(b.extents, splat(3.0f));
        CHECK(std::fabs(l.angle_a - 40.0f * s3::rs::from_bits(0x3c8efa35u)) < 1e-6f);
    }
    void bind_board(Key key) override {
        events.push_back("bind");
        CHECK_EQ(key, (Key{7, 11}));
    }
    void commit_bound_board() override { events.push_back("bound"); }
    void commit_free_board(Frame) override { events.push_back("free"); }
    void reset_board() override { events.push_back("reset"); }
    void update_toolkit(ToolkitInput in) override {
        events.push_back("toolkit");
        toolkit = in;
    }
    float deck_half_wheelbase() const override { return 0.3f; }
    void submit_ground_query(ground_query::GroundQueryPacket) override { events.push_back("submit"); }
};

S3_TEST(launch_then_skeleton_then_board_then_toolkit_then_edges) {
    State s{};
    auto m = motion();
    m.launch_1076 = true;
    Recorder r;
    r.found = true;
    r.accepted = true;
    auto res = sync(s, m, r);
    CHECK(res.is_ok() && res.unwrap() == false);
    CHECK_EQ(r.events, (std::vector<std::string>{"prepare", "air", "skeleton", "probe", "bone23", "classify",
                                                 "board_query", "bind", "bound", "toolkit", "edges"}));
    CHECK_EQ(s.frame_80[3], (Vector{0.0f, 2.0f, 0.0f, 1.0f}));
    CHECK_EQ(s.counter_152, 7u);
    CHECK_EQ(s.counter_156, 11u);
    CHECK(s.flags_144_to_150[0] && s.flags_144_to_150[2] && s.flags_144_to_150[3]);
    CHECK_EQ(s3::rs::unwrap(r.toolkit)._0, (std::array<Vector, 7>{s.frame_80[3], splat(3.0f), splat(2.0f), splat(1.0f),
                                                                   splat(0.0f), FRAME[1], FRAME[0]}));
}

S3_TEST(failed_probe_still_updates_board_and_uses_free_action) {
    State s{};
    Recorder r;
    CHECK(sync(s, motion(), r).is_ok());
    CHECK_EQ(r.events, (std::vector<std::string>{"skeleton", "probe", "board_query", "free", "toolkit", "edges"}));
}

S3_TEST(last_spin_tick_still_rotates_and_landing_gate_clears_without_query) {
    State s{};
    s.flags_144_to_150[6] = true;
    s.flags_144_to_150[4] = true;
    s.duration_180 = 0.001f;
    s.angle_172 = 1.0f;
    s.angular_velocity_176 = 99.0f;
    Recorder r;
    r.p.elapsed_2664 = 0.6f;
    CHECK(sync(s, motion(), r).is_ok());
    CHECK(!s.flags_144_to_150[6]);
    CHECK(!s.flags_144_to_150[4]);
    CHECK_EQ(s.duration_180, 0.0f);
    CHECK_EQ(s.angle_172, 0.9f);
    CHECK(r.bone_frame[2][0] > 0.7f);
    CHECK_EQ(r.events, (std::vector<std::string>{"skeleton", "toolkit", "edges"}));
}

S3_TEST(selected_edge_submits_next_frame_packet_without_consuming_it) {
    State s{};
    Recorder r;
    r.edge = true;
    auto m = motion();
    m.contact_position_192 = FRAME[3];
    auto res = sync(s, m, r);
    CHECK(res.is_ok() && res.unwrap() == true);
    const std::vector<std::string> tail(r.events.end() - 3, r.events.end());
    CHECK_EQ(tail, (std::vector<std::string>{"toolkit", "edges", "submit"}));
}
}  // namespace
