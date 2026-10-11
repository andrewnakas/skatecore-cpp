// Port of skate-core/src/player/offboard/contact_toolkit/candidate.rs (whole file).
// Candidate record82D82380 and slope-projected approaches82D84D38/82D85070.
#pragma once
#include <vector>

#include "s3/core/player/offboard/contact_toolkit/profile.hpp"

namespace s3::player::offboard::contact_toolkit::candidate {

struct Candidate {
    Vector position = ZERO;
    Vector normal = UP;
    Vector direction = ZERO;
    std::uint32_t flags = 0;
    float low = 0.0f;
    float high = 0.0f;
    float order = 0.0f;
    std::int32_t segment = 0;
    std::uint32_t kind = 0;
};

struct Builder {
    Input input{};
    const profile::Profile* profile = nullptr;
    float obstruction = 0.0f;
    std::vector<Candidate> candidates;

    void add(Vector position, Vector normal, Vector direction, std::int32_t segment, std::uint32_t flags,
             std::uint32_t kind);
    void point(Vector position, Vector normal, std::int32_t segment, std::uint32_t flags, std::uint32_t kind);
    std::array<float, 2> local(Vector p) const;
    void approach_up(std::size_t index, std::uint32_t flags, float slope);
    void approach_down(std::int32_t index, std::uint32_t flags, float slope, std::uint32_t kind);
    void advance(float distance);
    Vector next_normal(std::size_t i) const;
};
float up_slope(const Input& input, float height);
float down_slope(const Input& input, float height);
float between_slope(const Input& input, Vector a, Vector b);

}  // namespace s3::player::offboard::contact_toolkit::candidate
