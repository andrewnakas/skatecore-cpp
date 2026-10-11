// Port of skate-core/src/physics/world_contact/mod.rs.
// TU3 world-contact production and native temporary-contact retention.
// General primitive dispatch remains a separate provider dependency.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"
#include "s3/core/physics/world_contact/axes.hpp"
#include "s3/core/physics/world_contact/box_feature.hpp"
#include "s3/core/physics/world_contact/buffer.hpp"
#include "s3/core/physics/world_contact/dispatch.hpp"
#include "s3/core/physics/world_contact/face_clip.hpp"
#include "s3/core/physics/world_contact/face_edge.hpp"
#include "s3/core/physics/world_contact/feature.hpp"
#include "s3/core/physics/world_contact/point_feature.hpp"
#include "s3/core/physics/world_contact/primitive_pair.hpp"
#include "s3/core/physics/world_contact/primitive_query.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"
#include "s3/core/physics/world_contact/projection.hpp"
#include "s3/core/physics/world_contact/reduction.hpp"
#include "s3/core/physics/world_contact/segment_face.hpp"
#include "s3/core/physics/world_contact/segment_prism.hpp"
#include "s3/core/physics/world_contact/triangle_feature.hpp"

namespace s3::physics::world_contact {
// MaximumFeature, FeaturePrism and ContactRecord are declared in world_contact/arithmetic.hpp.
using primitive_pair::primitive_pair_contacts;
using primitive_pair::PrimitivePairSettings;

using axes::separating_axis_candidates;
using box_feature::box_maximum_feature;
using buffer::ContactBuffer;
using dispatch::find_feature_intersection_prism;
using face_clip::clamp_point_to_feature;
using face_clip::clip_segment_to_feature;
using face_edge::intersect_feature_corner_edge;
using feature::capsule_maximum_feature;
using feature::initialize_feature_segment;
using point_feature::closest_feature_segment;
using point_feature::intersect_point_face;
using primitive_query::ContactPrimitive;
using primitive_query::primitive_triangle_world_contacts;
using primitive_query::PrimitiveContactManifold;
using primitive_query::transform_triangle_volume;
using primitive_query::triangle_from_volume;
using projection::best_separating_direction;
using projection::PrimitiveKind;
using projection::project_direction;
using projection::project_directions;
using segment_face::intersect_segment_face;
using segment_prism::intersect_feature_segments;
using triangle_feature::build_feature_edge_planes;
using triangle_feature::triangle_maximum_feature;
using reduction::coplanar_contacts;
using reduction::select_contact_points;
}  // namespace s3::physics::world_contact
