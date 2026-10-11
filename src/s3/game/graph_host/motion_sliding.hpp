// Port of skate-game/src/graph_host/motion_sliding.rs (module `motion_sliding`).
//! PowerSliding Update82BB28E8, instance82BB2D70 and helper82595930.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/input/set_turning.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_animation {
class MotionAnimation;
}

namespace s3::game::graph_host::motion_sliding {

using motion_animation::MotionAnimation;
using s3::data::state_graph_attributes::Attributes;
using s3::input::set_turning::SlideLatch;
using s3::point_graph::PointGraph;

/// Fields private in Rust.
struct Settings {
    float speed_threshold = 0.0f;
    float well_into_slide = 0.0f;
    PointGraph<4> speed_factor;
    PointGraph<4> time_threshold;
    PointGraph<4> threshold;
    PointGraph<8> speed_to_lean;
    PointGraph<8> phys_to_anim_spin;
    float smooth = 0.0f;
    float minimum_entry = 0.0f;
    float turn = 0.0f;

    static rs::Result<Settings, std::string> load(const s3::data::Collections& data);
};

/// Rust `enum Operation`; each variant is a nested struct held in `value`.
struct Operation {
    struct Update {};
    struct Create {
        bool right = false;
    };
    struct ManualAttribute {};
    struct Deceleration {};
    struct Spin {};
    struct Candidate {};
    struct IsPowerSliding {};

    std::variant<Update, Create, ManualAttribute, Deceleration, Spin, Candidate, IsPowerSliding> value;

    Operation(Update v) : value(v) {}
    Operation(Create v) : value(v) {}
    Operation(ManualAttribute v) : value(v) {}
    Operation(Deceleration v) : value(v) {}
    Operation(Spin v) : value(v) {}
    Operation(Candidate v) : value(v) {}
    Operation(IsPowerSliding v) : value(v) {}

    static std::optional<Operation> parse(const Attributes& a);
};
S3_DERIVE_EQ_EMPTY(Operation::Update)
S3_DERIVE_EQ(Operation::Create, right)
S3_DERIVE_EQ_EMPTY(Operation::ManualAttribute)
S3_DERIVE_EQ_EMPTY(Operation::Deceleration)
S3_DERIVE_EQ_EMPTY(Operation::Spin)
S3_DERIVE_EQ_EMPTY(Operation::Candidate)
S3_DERIVE_EQ_EMPTY(Operation::IsPowerSliding)
S3_DERIVE_EQ(Operation, value)

///Native instance8..20; the three high flags initialize clear. (Fields private in Rust.)
struct State {
    float not_sliding_time = 0.0f;
    float previous_right = 0.0f;
    float previous_left = 0.0f;
    bool was_ground = false;
    bool was_right_start = false;
    bool was_left_start = false;

    void update(const MotionAnimation& animation, std::uint32_t category, float speed, float direction, float dt,
                const Settings& settings, SlideLatch& output);
};

///82595930 normalizes Motion80 with two root and reciprocal refinements,
/// then dots the authored Reckoning Z, reversing for Motion273.
float direction(std::array<float, 4> velocity, std::array<float, 4> z, bool flipped);

/// CreateSlide Update82BB2F60, ordered packet values (Slide, Turn).
std::array<float, 2> create(const SlideLatch& latch, bool authored_right, std::optional<float> right_intent,
                            std::optional<float> left_intent, const Settings& settings);

/// PowerSlideDecel82BB31B0: inverse Ground basis rotates velocity without
/// translation. Source zeros local Y before measuring the horizontal speed.
float deceleration(float& previous, std::array<float, 4> velocity, const std::array<std::array<float, 4>, 4>& ground,
                   const Settings& settings);

/// PowerSlideSpin Begin82BB33C8; the input is GetSlideDirection82595930.
inline float spin(float direction, const Settings& settings) { return -settings.phys_to_anim_spin.evaluate(direction); }

}  // namespace s3::game::graph_host::motion_sliding
