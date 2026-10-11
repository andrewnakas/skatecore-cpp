// Port of the #[cfg(test)] modules trick_name_tests and spin_tests in
// skate-game/src/scoring_runtime.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/game/scoring_runtime.hpp"

using namespace s3::game::scoring_runtime;
namespace rs = s3::rs;

namespace {
std::string concat(const char* a, const char* b) { return std::string(a) + b; }
}  // namespace

// ---- trick_name_tests ----

/// The user-visible symptom this fixes: a body flip was displayed as a bare air,
/// because nothing ever appended the flip token 82DA8EB8's +2348 selects.
S3_TEST(a_body_flip_is_named_and_takes_its_side_from_the_flip_direction) {
    CHECK_EQ(compose_trick_name(nullptr, 0, 1, false, false, true),
             "#" + concat(trick_tokens::AIR, " ") + trick_tokens::FRONTFLIP);
    CHECK_EQ(compose_trick_name(nullptr, 0, -1, false, false, true),
             "#" + concat(trick_tokens::AIR, " ") + trick_tokens::BACKFLIP);
    // A flip with no grab to record its side leaves +2348 at zero, and retail appends
    // nothing at all rather than guessing a side.
    CHECK_EQ(compose_trick_name(nullptr, 0, 0, false, false, true), "#" + std::string(trick_tokens::AIR));
}

/// 82DA8BE0 keeps signed half-turns; the display prints whole degrees.
S3_TEST(a_spin_prints_degrees_and_picks_its_side_by_parity) {
    CHECK_EQ(compose_trick_name("ID_TRICK_FLIP_KICKFLIP", 2, 0, false, false, true),
             std::string("#ID_TRICK_FLIP_KICKFLIP 360"));
    CHECK_EQ(compose_trick_name("ID_TRICK_FLIP_KICKFLIP", -3, 0, false, false, true),
             std::string("#ID_TRICK_FLIP_KICKFLIP 540"));
    // Unnamed spins fall back to the FS/BS tokens, chosen by switch ^ sign ^ goofy.
    struct Case {
        int turns;
        bool switch_;
        bool goofy;
        const char* expected;
    };
    for (const Case c : {Case{1, false, false, trick_tokens::FS_SPIN}, Case{-1, false, false, trick_tokens::BS_SPIN},
                         Case{1, true, false, trick_tokens::BS_SPIN}, Case{1, false, true, trick_tokens::BS_SPIN},
                         Case{-1, true, true, trick_tokens::BS_SPIN}}) {
        CHECK_EQ(compose_trick_name(nullptr, c.turns, 0, c.switch_, c.goofy, true),
                 "#" + std::string(c.expected) + " 180");
    }
}

/// 825E51A0 refuses the spin decoration for a trick class that may not carry one, so
/// a manual (TrickType 8) never reads "Manual 180" and a handplant (6) never gains a
/// spin from the air before it.
S3_TEST(only_the_admitted_trick_classes_carry_a_spin) {
    for (std::uint32_t admitted : {1u, 2u, 3u, 4u, 7u, 10u, 12u, 13u}) {
        CHECK(decorates_spin(admitted));
    }
    for (std::uint32_t refused : {0u, 5u, 6u, 8u, 9u, 11u, 14u}) {
        CHECK(!decorates_spin(refused));
    }
    CHECK_EQ(compose_trick_name("ID_TRICK_GROUND_TRICK_MANUAL", 1, 0, false, false, false),
             std::string("#ID_TRICK_GROUND_TRICK_MANUAL"));
    // The flip suffix is *not* behind this gate -- 0x825E57A8 tests only the sign.
    CHECK_EQ(compose_trick_name("ID_TRICK_BS_HANDPLANT", 2, 0, false, false, false),
             std::string("#ID_TRICK_BS_HANDPLANT"));
}

/// Everything after the single leading `#` has to be tokens the text layer can look
/// up, so the name must never carry a second `#` or a double space.
S3_TEST(the_composed_name_is_one_hash_and_resolvable_tokens) {
    const std::string name = compose_trick_name("ID_TRICK_FLIP_KICKFLIP", 2, 1, true, false, true);
    std::size_t hashes = 0;
    for (char c : name) hashes += c == '#';
    CHECK_EQ(hashes, std::size_t(1));
    CHECK(name[0] == '#');
    CHECK(name.find("  ") == std::string::npos);
    std::string rest = name;
    while (!rest.empty() && rest[0] == '#') rest.erase(0, 1);
    std::vector<std::string> parts;
    std::size_t at = 0;
    for (;;) {
        const std::size_t sp = rest.find(' ', at);
        parts.push_back(rest.substr(at, sp == std::string::npos ? std::string::npos : sp - at));
        if (sp == std::string::npos) break;
        at = sp + 1;
    }
    CHECK((parts == std::vector<std::string>{"ID_TRICK_FLIP_KICKFLIP", "360", trick_tokens::FRONTFLIP}));
}

// ---- spin_tests ----

namespace {
const std::array<float, 3> UP = {0.0f, 1.0f, 0.0f};

std::array<float, 3> yaw(float degrees) {
    const float radians = rs::to_radians(degrees);
    return {std::sin(radians), 0.0f, std::cos(radians)};
}
}  // namespace

/// A rotation about the frame's own up axis reads back as that rotation.
S3_TEST(a_yaw_about_the_frames_own_up_is_measured) {
    for (float step : {1.0f, 15.0f, 45.0f, 89.0f, -30.0f}) {
        const float measured = rs::to_degrees(spin_angle(UP, yaw(0.0f), yaw(step)));
        CHECK(std::fabs(std::fabs(measured) - std::fabs(step)) < 0.01f);
    }
}

/// Accumulating a full turn in steps totals one turn, which is what makes a 360 read
/// as 360 rather than wrapping away to nothing.
S3_TEST(stepwise_rotation_accumulates_to_a_whole_turn) {
    float total = 0.0f;
    auto reference = yaw(0.0f);
    for (int step = 1; step <= 72; ++step) {
        const auto next = yaw(float(step) * 5.0f);
        total += spin_angle(UP, reference, next);
        reference = next;
    }
    CHECK(std::fabs(std::fabs(rs::to_degrees(total)) - 360.0f) < 0.5f);
}

/// 82DAC9D0 returns zero rather than a bogus angle when the forward moves more than a
/// quarter turn in a single frame, and when either vector is degenerate.
S3_TEST(a_discontinuity_or_a_degenerate_frame_scores_nothing) {
    CHECK_EQ(spin_angle(UP, yaw(0.0f), yaw(91.0f)), 0.0f);
    CHECK_EQ(spin_angle(UP, yaw(0.0f), yaw(180.0f)), 0.0f);
    CHECK_EQ(spin_angle(UP, yaw(0.0f), {0.0f, 0.0f, 0.0f}), 0.0f);
    CHECK_EQ(spin_angle({0.0f, 0.0f, 0.0f}, yaw(0.0f), yaw(10.0f)), 0.0f);
    // A forward parallel to up leaves no yaw to measure.
    CHECK_EQ(spin_angle(UP, yaw(0.0f), UP), 0.0f);
}

/// The board may rotate all it likes: only the transform handed in is measured. This is
/// the tre-flip case -- a 360 shove-it with the rider square on must read zero.
S3_TEST(a_stationary_rider_registers_no_spin) {
    float total = 0.0f;
    const auto reference = yaw(0.0f);
    for (int i = 0; i < 60; ++i) {
        total += spin_angle(UP, reference, reference);
    }
    CHECK_EQ(total, 0.0f);
}
