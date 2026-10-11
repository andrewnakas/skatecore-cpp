// Port of skate-core/src/physics/skeleton_output/board.rs.
// Board portion of original TU3 physical pose worker82DB993C..A55C.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::skeleton_output::board {

using Transform = skeleton_animation_record::AnimationPartTransform;

struct Settings {
    float truck_tilt_scalar = 0.0f;
    float truck_tilt_max_angle = 0.0f;
    float truck_tilt_wobble_scalar = 0.0f;
    float truck_displacement_max = 0.0f;
};

struct BoneIndices {
    std::size_t front_truck = 0;
    std::size_t back_truck = 0;
    std::size_t front_left_wheel = 0;
    std::size_t front_right_wheel = 0;
    std::size_t back_left_wheel = 0;
    std::size_t back_right_wheel = 0;

    std::array<std::size_t, 6> all() const {
        return {front_truck, back_truck, front_left_wheel, front_right_wheel, back_left_wheel,
                back_right_wheel};
    }
};

struct Input {
    /// Raw dynamic-body matrices82585B58, native parts0..5; not82585CB0.
    const std::array<Transform, 6>& bodies;
    /// SkeletonPhysicalRecord.part0, captured separately by82DB670C.
    const Transform& skeleton_board;
    /// SkateboardBody7840/7904, not7712/7776 steering-drive base frames.
    const std::array<Transform, 2>& truck_frames;
    float deck_wobble_tilt;
    float deck_wobble_squish;
    /// CalculateAverageWheelCompressions82C08968, front8372/back8376.
    std::array<float, 2> average_compression;
};

void publish(const Input& input, const Settings& settings, BoneIndices bones, Slice<Transform> locals);

}  // namespace s3::physics::skeleton_output::board
