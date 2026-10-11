// Port of skate-core/src/player/offboard/contact_toolkit/samples.rs (whole file).
// Contact projection/classification82D81F80. No synthesized floor samples.
#pragma once
#include <vector>

#include "s3/core/player/offboard/contact_toolkit/probes.hpp"

namespace s3::player::offboard::contact_toolkit {

struct ContactSample {
    Vector position{};
    Vector normal{};
    float forward_distance = 0.0f;
    float height = 0.0f;
    std::uint32_t flags = 0;
    float sort_distance = 0.0f;
};
S3_DERIVE_EQ(ContactSample, position, normal, forward_distance, height, flags, sort_distance)

struct Samples {
    std::vector<ContactSample> ground;      // 10832, capacity64
    std::vector<ContactSample> other;       // 14944
    std::size_t original_ground_count = 0;  // 14932

    bool insert(const Input& input, Vector position, Vector normal, std::uint32_t category, float sort_override,
                std::uint32_t provenance);
};

}  // namespace s3::player::offboard::contact_toolkit
