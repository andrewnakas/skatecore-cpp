// Port of skate-core/src/air/trajectory/mod.rs.
// TU3 trajectory selection82D67848/82D682E8/82D68800.
// Host world storage and batch ownership are independent of console layout.
#pragma once
#include "s3/core/air/trajectory/grind.hpp"
#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/core/air/trajectory/launch.hpp"
#include "s3/core/air/trajectory/math.hpp"
#include "s3/core/air/trajectory/prediction.hpp"
#include "s3/core/air/trajectory/query.hpp"
#include "s3/core/air/trajectory/scoring.hpp"
#include "s3/core/air/trajectory/selector.hpp"
#include "s3/core/air/trajectory/types.hpp"

namespace s3::air::trajectory {

// pub use prediction::{Prediction, QueryRequest, QueryResult, Trajectory};
using prediction::Prediction;
using prediction::QueryRequest;
using prediction::QueryResult;
using prediction::Trajectory;
// pub use query::{SurfaceHit, query_trajectory};
using query::query_trajectory;
using query::SurfaceHit;
// pub use selector::{Selection, TrajectorySelector, WorldWithoutGrindEdges};
using selector::Selection;
using selector::TrajectorySelector;
using selector::WorldWithoutGrindEdges;
// pub use types::{LaunchInfo, SelectorInput, SelectorSettings};
using types::LaunchInfo;
using types::SelectorInput;
using types::SelectorSettings;

}  // namespace s3::air::trajectory
