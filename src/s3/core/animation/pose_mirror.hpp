// Port of skate-core/src/animation/pose_mirror.rs.
// ACS immediate Mirror828CDAF8. Pairing comes from the stock hierarchy.
#pragma once
#include <cstdint>
#include <string>

#include "s3/core/animation/output.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::pose_mirror {

using output::Sqt;

rs::Result<rs::Unit, std::string> mirror(Slice<Sqt> pose, Slice<const std::int32_t> parents,
                                         Slice<const std::int32_t> partners, std::uint32_t trajectory_mode);

}  // namespace s3::animation::pose_mirror
