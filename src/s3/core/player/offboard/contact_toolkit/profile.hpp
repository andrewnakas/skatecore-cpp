// Port of skate-core/src/player/offboard/contact_toolkit/profile.rs (whole file).
// Profile cleanup82D830A8 and segment construction82D83438.
#pragma once
#include <utility>
#include <vector>

#include "s3/core/player/offboard/contact_toolkit/samples.hpp"

namespace s3::player::offboard::contact_toolkit::profile {

struct Segment {
    std::uint32_t kind = 0;
    Vector start{};
    Vector end{};
    Vector direction{};
    Vector normal{};
    float length = 0.0f;
};
struct Profile {
    std::vector<Segment> segments;
    std::size_t last_ground = 0;
};
std::pair<float, std::size_t> simplify(const Input& input, std::vector<ContactSample>& points);
/// Stable sort by sort_distance (Rust `sort_by` with partial_cmp, NaN == Equal).
void sort(std::vector<ContactSample>& points);
Profile build(const Input& input, const ContactSample* points, std::size_t count);

}  // namespace s3::player::offboard::contact_toolkit::profile
