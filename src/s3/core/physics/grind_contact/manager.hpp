// Port of skate-core/src/physics/grind_contact/manager.rs.
// Manager leaves recovered from original TU3, independent of host storage.
#pragma once
#include <optional>

#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::physics::grind_contact::manager {

///82D87460, after CalcNormalizedGrindDir82D37048.
V directed_tangent(V start, V end, V velocity, V previous);

struct GeometryInput {
    bool valid = false;
    std::uint32_t family = 0;
    std::uint32_t current_state = 0;
    std::uint32_t category = 0;
    std::uint32_t air_frames = 0;
    std::uint32_t geometry_kind = 0;
    std::uint32_t geometry_flags = 0;
    std::array<V, 2> far_points{};
    V upmost{};
    V high_side{};
    V point{};
    V direction{};
    V board_position{};
    V board_forward{};
    V velocity{};
    float deck_to_truck = 0.0f;
    float previous_exit_angle = 0.0f;
    V previous_exit_direction{};
    std::uint32_t flags = 0;
};

struct GeometryOutput {
    bool valid = false;
    std::uint32_t family = 0;
    std::uint32_t flags = 0;
};

///82D880D0, with the retained one-truck exclusion owned by the investigator.
GeometryOutput tweak_geometry(GeometryInput i, std::uint32_t& avoid_five_o_frames);

///82D8ACF0 / S2 ManageCopingAssistance82DDBC48. Update, then decrement, even
///on the activation frame. FrictionVsTime is a different pre-update producer.
float gravity_relief(float& timer, bool valid, std::uint32_t category, V tangent, V velocity, float dt,
                     const point_graph::PointGraph<4>& vertical, const point_graph::PointGraph<4>& linear);

struct JumpGeometry {
    std::uint32_t geometry_kind = 0;
    V high_side{};
    V normal{};
    V direction{};
    V upmost{};
    V point{};
};

struct Jumper {
    bool launched = false;
    std::uint32_t cooldown = 0;
    std::uint32_t family = 0;
    float energy = 0.0f;
    JumpGeometry geometry;

    ///82D8A318; original vector constants82139A10/82139A20 checked in memory.
    static Jumper default_();

    ///82D739D8: invalid investigation retains geometry. Return a flag to OR,
    ///not a replacement for the complete processed flags2476 word.
    std::uint32_t update(std::optional<JumpGeometry> geometry, std::uint32_t published_family);
};

}  // namespace s3::physics::grind_contact::manager
