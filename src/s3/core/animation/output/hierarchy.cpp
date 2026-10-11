// Port of skate-core/src/animation/output/hierarchy.rs.
#include "s3/core/animation/output/hierarchy.hpp"

#include <algorithm>

namespace s3::animation::output::hierarchy {

namespace {

rs::Result<rs::Unit, PoseBufferError> validate(std::size_t count, Slice<const std::int32_t> parents,
                                               std::int32_t detached_parent, std::size_t input_len,
                                               std::size_t output_len) {
    if (input_len < std::max<std::size_t>(count, 1)) {
        return rs::make_err(PoseBufferError(PoseBufferError::ShortInput{}));
    }
    if (output_len < std::max<std::size_t>(count, 1)) {
        return rs::make_err(PoseBufferError(PoseBufferError::ShortOutput{}));
    }
    if (parents.size() < count) {
        return rs::make_err(PoseBufferError(PoseBufferError::ShortParents{}));
    }
    for (std::size_t bone = 0; bone < count; ++bone) {
        const std::int32_t parent = parents[bone];
        if (parent != -1 && parent != detached_parent &&
            (parent < 0 || static_cast<std::size_t>(parent) >= output_len)) {
            return rs::make_err(PoseBufferError(PoseBufferError::InvalidParent{bone, parent}));
        }
    }
    return rs::Unit{};
}

NativeMatrix concatenate(const NativeMatrix& local, const NativeMatrix& parent) {
    NativeMatrix result = local;
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t lane = 0; lane < 4; ++lane) {
            const float first = row == 3 ? rs::mul_add(parent[0][lane], local[row][0], parent[3][lane])
                                         : parent[0][lane] * local[row][0];
            const float second = rs::mul_add(parent[1][lane], local[row][1], first);
            result[row][lane] = rs::mul_add(parent[2][lane], local[row][2], second);
        }
    }
    return result;
}

std::size_t clamp_count(std::int32_t bone_count) {
    return static_cast<std::size_t>(std::max<std::int32_t>(bone_count, 0));
}

}  // namespace

rs::Result<rs::Unit, PoseBufferError> compose_hierarchy(std::int32_t bone_count,
                                                       Slice<const std::int32_t> parents,
                                                       std::int32_t detached_parent,
                                                       Slice<const NativeMatrix> local,
                                                       Slice<NativeMatrix> destination) {
    const std::size_t count = clamp_count(bone_count);
    auto valid = validate(count, parents, detached_parent, local.size(), destination.size());
    if (valid.is_err()) return rs::make_err(valid.unwrap_err());
    destination[0] = local[0];
    for (std::size_t bone = 0; bone < count; ++bone) {
        const std::int32_t parent = parents[bone];
        destination[bone] = parent == -1 || parent == detached_parent
                                ? local[bone]
                                : concatenate(local[bone], destination[static_cast<std::size_t>(parent)]);
    }
    return rs::Unit{};
}

rs::Result<rs::Unit, PoseBufferError> compose_hierarchy_in_place(std::int32_t bone_count,
                                                                Slice<const std::int32_t> parents,
                                                                std::int32_t detached_parent,
                                                                Slice<NativeMatrix> matrices) {
    const std::size_t count = clamp_count(bone_count);
    auto valid = validate(count, parents, detached_parent, matrices.size(), matrices.size());
    if (valid.is_err()) return rs::make_err(valid.unwrap_err());
    for (std::size_t bone = 0; bone < count; ++bone) {
        const std::int32_t parent = parents[bone];
        if (parent != -1 && parent != detached_parent) {
            matrices[bone] = concatenate(matrices[bone], matrices[static_cast<std::size_t>(parent)]);
        }
    }
    return rs::Unit{};
}

}  // namespace s3::animation::output::hierarchy
