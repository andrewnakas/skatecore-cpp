// Port of skate-core/src/air/trajectory/scoring.rs (private module).
// TU3 scoring82D68FE8/82D68AF0/82D69790 for a world with no grind edges.
// That topology is supplied by the authored BoardWorld, not inferred from a
// failed collision. Every wall probe still queries real world geometry.
#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "s3/core/air/trajectory/grind.hpp"
#include "s3/core/air/trajectory/math.hpp"
#include "s3/core/air/trajectory/prediction.hpp"
#include "s3/core/air/trajectory/query.hpp"
#include "s3/core/physics/slice.hpp"
#include "s3/core/air/trajectory/types.hpp"

namespace s3::air::trajectory::scoring {

using math::Vector;
using prediction::Prediction;
using query::SurfaceHit;
using types::SelectorInput;
using types::SelectorSettings;

/// `impl FnMut(&mut Prediction, bool) -> Result<GrindEvaluation, String>`.
using GrindFn = std::function<rs::Result<grind::GrindEvaluation, std::string>(Prediction&, bool)>;
/// `impl FnMut(Vector, Vector, f32) -> Result<Option<SurfaceHit>, String>`.
using LineFn = std::function<rs::Result<std::optional<SurfaceHit>, std::string>(Vector, Vector, float)>;

/// pub(super)
struct Candidate {
    Prediction prediction{};
    Vector start_velocity{};      // candidate64+0, before wall adjustment
    Vector normal{};              // +16
    Vector collision_velocity{};  // +32, before wall adjustment
    Vector collision_position{};  // +48, before wall adjustment
    float score = 0.0f;
    float wall_score = 0.0f;
    bool wall_ride = false;
    std::optional<grind::GrindTarget> grind;
};

/// pub(super). `candidates` is a mutable slice (`&mut [Candidate]`).
rs::Result<bool, std::string> score(physics::Slice<Candidate> candidates, std::uint16_t pass, bool adjusted_on_vert,
                                    SelectorInput input, const SelectorSettings& s, GrindFn grind, LineFn line);

/// (private)
float apex_time(Prediction p);
/// (private)
rs::Result<float, std::string> wall_score(Prediction& p, bool reject_grind, const SelectorSettings& s,
                                          float& highest, LineFn& line);

}  // namespace s3::air::trajectory::scoring
