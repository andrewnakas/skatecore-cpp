// Port of skate-core/src/input/set_turning.rs.
// SetTurning enter 82BB3810 and update 82BB3830 at resolved component boundaries.
// This is a reusable node operation; the loaded graph owns its activation.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

#include "s3/core/input/turn_remap.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::set_turning {

using point_graph::PointGraph;
using turn_remap::TurnRemap;

/// One retained native slide record (SpecificMotionGraph3204..3220).
/// Both SetTurning and authored slide handlers read and mutate this owner.
/// Reserved bits stay intact; reset82595450..78 only clears the defined bits.
struct SlideLatch {
    // Private in Rust (mutated directly by `update` in the same module).
    std::array<std::uint32_t, 5> words{};

    bool captured_fakie() const { return (words[4] & 0x80000000u) != 0; }
    bool candidate_enabled() const { return (words[4] & 0x40000000u) != 0; }
    void set_candidate_enabled(bool enabled) {
        words[4] = (words[4] & ~0x40000000u) | (std::uint32_t(enabled) << 30);
    }
    float elapsed(bool right) const { return rs::from_bits(words[right ? 2 : 0]); }
    bool start(bool right) const { return (words[right ? 3 : 1] & 0x80000000u) != 0; }
    bool end(bool right) const { return (words[right ? 3 : 1] & 0x40000000u) != 0; }
    void set_start(bool right, bool value) {
        std::uint32_t& word = words[right ? 3 : 1];
        word = (word & ~0x80000000u) | (std::uint32_t(value) << 31);
    }
    void set_end(bool right, bool value) {
        std::uint32_t& word = words[right ? 3 : 1];
        word = (word & ~0x40000000u) | (std::uint32_t(value) << 30);
    }
    void advance_elapsed(bool right, float dt) {
        std::uint32_t& word = words[right ? 2 : 0];
        word = rs::to_bits(dt + rs::from_bits(word));
    }
    /// CreateSlide Begin82BB2E88; do not clear start/end/grab/candidate flags.
    void begin_slide(bool fakie) {
        words[0] = 0;
        words[2] = 0;
        words[4] = (words[4] & ~0x80000000u) | (std::uint32_t(fakie) << 31);
    }
    /// GrabSlide82BBB848 maps authored side through the captured stance.
    void grab(bool authored_right) {
        const bool right = authored_right ^ captured_fakie();
        words[right ? 3 : 1] |= 0x20000000u;
    }
    /// ShouldLeaveSlide82BA7130 uses captured, not current, stance.
    bool should_leave(bool authored_right) const { return end(authored_right ^ captured_fakie()); }
    void reset() {
        words[0] = 0;
        words[2] = 0;
        words[1] &= 0x1fffffffu;
        words[3] &= 0x1fffffffu;
        words[4] &= 0x3fffffffu;
    }
};
S3_DERIVE_EQ(SlideLatch, words)

struct State {
    float elapsed = 0.0f;
    float smoothed = 0.0f;
    std::uint32_t mode = 0;

    /// Native enter and allocator 82BB4138 use the same three initial values.
    void enter() {
        elapsed = 0.0f;
        smoothed = 0.0f;
        mode = 2;
    }
};

struct Settings {
    /// Native helper mode 0 and mode 1, in that order.
    std::array<TurnRemap, 2> remaps{};
    PointGraph<8> speed_tuck;
    PointGraph<8> blend;
    float speed_threshold = 0.0f;
    float maximum_delta = 0.0f;
    /// Global collection slot 332, layout 1680.
    float override_turn = 0.0f;
};

/// Resolved physical output bundle+56 and bundle+4 inputs. Offset names avoid
/// assigning unverified meanings to upstream physical producer fields.
struct Physical {
    float field_32 = 0.0f;
    float field_36 = 0.0f;
    float field_52 = 0.0f;
    float field_56 = 0.0f;
    float field_60 = 0.0f;
    float body_168 = 0.0f;
};

struct Intents {
    std::optional<float> fakie_turn;
    /// Native keys 830C067C and 830C05F4, respectively.
    std::optional<float> mode_0_slide;
    std::optional<float> mode_1_slide;
};

/// Skeleton selectors bind to the decoded node parameter keys. Packet Turn and
/// Slide are native fixed attribute keys. Emissions occur in native call order.
enum class Attribute {
    Angle,
    Direction,
    Quickness,
    Speed,
    Holding,
    Turn,
    Slide,
};

/// `latch` preserves all five words returned by motion graph virtual+64.
/// `stance` is the component's virtual+12 and virtual+28 result pair.
/// `dt` is the graph context clock virtual+40 result, not a guessed fixed tick.
void update(State& state, SlideLatch& latch, Physical physical, std::pair<bool, bool> stance, float dt,
            const Settings& settings, Intents intents, const std::function<void(Attribute, float)>& emit);

}  // namespace s3::input::set_turning
