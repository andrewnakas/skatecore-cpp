// Port of simref sim/src/camera/shot_data.rs.
//! Immutable stock shot definitions. Field order/units follow TU382CA4660.
#pragma once
#include <map>
#include <string>

#include "s3/core/camera/shot_manager.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::camera::shot_data {

/// Rust tuple struct `StockShots(BTreeMap<String, ShotDefinition>)`.
struct StockShots : s3::camera::ShotDatabase {
    std::map<std::string, s3::camera::ShotDefinition> _0;

    static rs::Result<StockShots, std::string> from_collections(const s3::data::Collections& data);

    rs::Result<s3::camera::ShotDefinition, std::string> load(const std::string& name) const override;
};

}  // namespace s3::game::camera::shot_data
