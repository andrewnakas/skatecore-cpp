// Port of skate-core/src/animation/output/hierarchy.rs.
#pragma once
#include <cstdint>

#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::output::hierarchy {

/// TU3 0x828D3B58: local matrices to hierarchy-composed matrices.
///
/// `detached_parent` is native r5 (hierarchy +8 at the caller). A parent equal to
/// it, or -1, causes a verbatim local copy. No world transform is introduced.
/// The first matrix is copied even for a nonpositive count. Other output slots
/// retain caller state until visited, including when a forward parent is read.
rs::Result<rs::Unit, PoseBufferError> compose_hierarchy(std::int32_t bone_count,
                                                       Slice<const std::int32_t> parents,
                                                       std::int32_t detached_parent,
                                                       Slice<const NativeMatrix> local,
                                                       Slice<NativeMatrix> destination);

/// The same native function with identical source/destination matrix pointers.
rs::Result<rs::Unit, PoseBufferError> compose_hierarchy_in_place(std::int32_t bone_count,
                                                                Slice<const std::int32_t> parents,
                                                                std::int32_t detached_parent,
                                                                Slice<NativeMatrix> matrices);

}  // namespace s3::animation::output::hierarchy

namespace s3::animation::output {
// pub use hierarchy::{compose_hierarchy, compose_hierarchy_in_place}; (repeated in output.hpp)
using hierarchy::compose_hierarchy;
using hierarchy::compose_hierarchy_in_place;
}  // namespace s3::animation::output
