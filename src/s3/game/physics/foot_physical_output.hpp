// Port of skate-game/src/physics/foot_physical_output.rs.
//! Persistent original physical-foot output owner. Call after final physical
//! record correction, at Skeleton::FillPhysOut82BE2138/2140 publication time.
#pragma once
#include <string>

#include "s3/core/physics/foot_physical_output.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::foot_physical_output {

struct FootPhysicalOutputs {
    // Private in Rust.
    sc::physics::foot_physical_output::FootPhysicalState state{};
    sc::physics::foot_physical_output::FootPhysicalSettings settings{};
    // Public in Rust.
    sc::physics::foot_physical_output::FootPhysicalOutput output{};

    static rs::Result<FootPhysicalOutputs, std::string> load(const sd::Collections& data);
    sc::physics::foot_physical_output::FootPhysicalOutput publish(
        const sc::physics::skeleton_body::SkeletonPhysicalRecord& record, float dt);
};

}  // namespace s3::game::physics::foot_physical_output
