// Port of skate-core/src/animation/output/mod.rs.
// Native TU3 pose-output kernels. These do not evaluate clips or choose graphs.
#pragma once
#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/output/hierarchy.hpp"
#include "s3/core/animation/output/sqt.hpp"

namespace s3::animation::output {
// pub use hierarchy::{compose_hierarchy, compose_hierarchy_in_place};
using hierarchy::compose_hierarchy;
using hierarchy::compose_hierarchy_in_place;
// pub use sqt::{Sqt, sqt_to_local, sqt_to_matrix};
using sqt::Sqt;
using sqt::sqt_to_local;
using sqt::sqt_to_matrix;
}  // namespace s3::animation::output
