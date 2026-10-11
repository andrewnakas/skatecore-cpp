// Port of skate-game/src/physics/air_trajectory/grind.rs.
//! Real static University provider -> native trajectory admission -> landing orientation.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/air/trajectory/grind.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/grind_world.hpp"

namespace s3::game::physics::air_trajectory::grind {

using V = std::array<float, 4>;
using ::s3::air::trajectory::Prediction;
using ::s3::air::trajectory::grind::GrindAssistLimits;
using ::s3::air::trajectory::grind::GrindEvaluation;
using ::s3::game::grind_world::StaticProvider;
using ::s3::physics::board_world::BoardWorld;
using ::s3::player::input_phase::ProcessedPhysicsInput;

struct GrindContext {
    V board_position{};
    V body_position{};
    std::array<std::uint32_t, 2> actor{};

    static GrindContext from_processed(const ProcessedPhysicsInput& p, V board_position);
};

struct Settings {
    // Private in Rust.
    GrindAssistLimits limits{};
    ::s3::point_graph::PointGraph<8> height{};
    float padding = 0.0f;
    float maximum_adjust = 0.0f;
    float velocity_scalar = 0.0f;
    float max_angle = 0.0f;
    float score = 0.0f;
    float truck_distance = 0.0f;
    float penalty_domain = 0.0f;

    static rs::Result<Settings, std::string> load(const sd::Collections& data);
    rs::Result<GrindEvaluation, std::string> evaluate(Prediction& prediction, bool acquire, const BoardWorld& world,
                                                      const StaticProvider& provider, std::vector<std::size_t>& nearby,
                                                      float lock_distance, GrindContext context) const;
};

// Private helpers in Rust.
float dot(V a, V b);
rs::Result<::s3::point_graph::PointGraph<8>, std::string> graph8(const sd::Collections& data, const std::string& class_,
                                                                 const std::string& name, bool negative);
V sub(V a, V b);
V scale(V a, float s);
V cross(V a, V b);
float inverse(float square);
float length_square(float square);
V unit(V a);

}  // namespace s3::game::physics::air_trajectory::grind
