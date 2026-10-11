// Port of skate-core/src/physics/grind_contact/entry.rs.
// TU3 82D86F00 airborne entry and 82D872B8 impact admission.
// The caller retains this result until Grind::PreUpdate82D40AF8 consumes it.
#pragma once
#include <vector>

#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::physics::grind_contact::entry {

struct Input {
    bool valid = false;
    std::uint32_t kind = 0;
    std::uint32_t category = 0;
    std::uint32_t previous_state_2504 = 0;
    float speed = 0.0f;
    float balance_2720 = 0.0f;
    V direction{};
    V normal{};
    V up{};
    V board_velocity{};
    V air_velocity{};
    std::uint32_t surface_kind = 0;
    V high_side{};
    /// physics_grinds/VertEngagementHelpVsVelY, X144/Y160. (Rust `&'a PointGraph<4>`.)
    const point_graph::PointGraph<4>* vertical_help = nullptr;
    /// Stock wipeout collection layout256, checked by82D872B8.
    float max_delta = 0.0f;
    std::uint32_t flags = 0;
    V previous_entry_velocity{};
};

struct Output {
    bool valid = false;
    std::uint32_t flags = 0;
    V entry_velocity{};
    float impact_speed = 0.0f;
    /// Wipeout reason offsets minus20. Each request also sets input2468 bit18.
    std::vector<std::size_t> wipeout_reasons;
};

struct Engagement {
    std::uint32_t tipslide_frames = 0;

    ///82D86DE8: retain flags/velocity when invalid; reset impact every update.
    Output update(const Input& input);
};

V grounded_velocity(std::uint32_t kind, V direction, V velocity, float balance, float speed,
                    const point_graph::PointGraph<4>& vertical_help);

V airborne_velocity(std::uint32_t kind, V direction, V normal, V velocity);

/// Native reason indices: byte33 steep entry,29 excessive correction,34
/// excessive horizontal impact. Retain both impact requests when both fire.
std::vector<std::size_t> airborne_rejections(V direction, V up, V board_velocity, V air_velocity, V corrected,
                                             std::uint32_t surface_kind, V surface_side, float max_delta);

}  // namespace s3::physics::grind_contact::entry
