// Port of skate-game/src/graph_host/motion_offboard/body_tweak.rs (module `motion::body_tweak`).
//! TU3 OffboardBodyTweakBlend: ctor82BBAB98, Begin82BBACC0,
//! Update82BBACE0, End82BBB418, instance82BBB4D0. Original IDA only.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_wipeout.hpp"

namespace s3::game::graph_host::motion::body_tweak {

using motion_animation::MotionAnimation;
using motion_wipeout::Controls;
using s3::data::state_graph_attributes::Attributes;

inline constexpr std::string_view CHANNEL = "AirBodyTweak";  // initializer82F87578 ->830BE028

struct Settings {
    std::string nb_cycle;
    std::string nb_into;
    std::string br_cycle;
    std::string br_into;

    static Settings parse(const Attributes& a);
};
S3_DERIVE_EQ(Settings, nb_cycle, nb_into, br_cycle, br_into)

struct Physical {
    float time_to_land = 0.0f;
    float scalar_92 = 0.0f;
    bool board_held = false;
};

/// Rust-private fields and helpers are public here (its tests read them).
struct State {
    std::uint32_t ticks = 0;
    std::array<float, 2> axes{};
    bool into_started = false;
    bool cycle_started = false;
    bool released = false;

    void begin();
    rs::Result<rs::Unit, std::string> update(const Settings& settings, MotionAnimation& animation,
                                             Controls& controls, Physical physical);
    /// Private in Rust. Returns (directed, centered).
    std::pair<bool, bool> gates(std::optional<float> x, std::optional<float> y, bool request, float scalar_92);
    /// Private in Rust.
    void filter(float x, float y, bool centered);
    void end(MotionAnimation& animation);
};

}  // namespace s3::game::graph_host::motion::body_tweak
