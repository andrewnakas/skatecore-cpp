// Port of skate-game/src/graph_host/motion_air_leg.rs (module `motion_air_leg`).
//! Stock ControlAirLegExtension configuration82BAF450 and settings Globals336.
#pragma once
#include <string>

#include "s3/core/animation/air_leg_extension.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_air_leg {

using s3::animation::air_leg_extension::Bone;
using s3::animation::air_leg_extension::Settings;
using s3::animation::output::attributes::AttributeName;
using s3::data::state_graph_attributes::Attributes;

struct Operation {
    AttributeName height;
    Bone bone = Bone::Board;

    static Operation parse(const Attributes& a);
};
S3_DERIVE_EQ(Operation, height, bone)

rs::Result<Settings, std::string> load(const s3::data::Collections& data);

}  // namespace s3::game::graph_host::motion_air_leg
