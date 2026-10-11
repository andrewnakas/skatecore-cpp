// Port of skate-core/src/input/gesture.rs.
// TU3 pattern recognition, 82697168/826972B8/826974A8.
// Points are supplied in PAT authoring order. Native 82696B18 pushes them
// to the front of a ring buffer, then the matcher walks that buffer backwards.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3::input::gesture {

struct Pattern {
    std::string name;
    std::vector<std::array<float, 2>> points;
    float tolerance_squared = 0.0f;
};
S3_DERIVE_EQ(Pattern, name, points, tolerance_squared)

struct Settings {
    /// Recognizer config byte 33. Supplied by the owner, never inferred here.
    std::uint8_t maximum_misses = 0;
    std::uint32_t difficulty = 0;
};

/// Private in Rust.
struct Node {
    bool active = false;
    bool complete = false;
    std::size_t next = 0;
    std::uint16_t elapsed = 0;
    std::uint8_t misses = 0;
    float distance = 0.0f;

    void tick(const Pattern& pattern, std::array<float, 2> sample, std::uint8_t maximum_misses);
    float score(std::size_t count) const;
};

struct Recognition {
    std::size_t pattern = 0;
    float strength = 0.0f;
    float distance = 0.0f;
    float elapsed = 0.0f;
};
S3_DERIVE_EQ(Recognition, pattern, strength, distance, elapsed)

class Recognizer {
public:
    static rs::Result<Recognizer, std::string> new_(std::vector<Pattern> patterns);

    const std::vector<Pattern>& patterns() const { return patterns_; }

    /// Held messages are checked before recognition in 826962D8. Native point
    /// zero is the final authored coordinate (82699738 push-front).
    std::optional<std::size_t> held(std::array<float, 2> sample);

    std::optional<Recognition> sample(std::array<float, 2> sample, Settings settings);

private:
    std::vector<Pattern> patterns_;
    std::vector<Node> nodes_;
    bool has_previous_sample_ = false;
    bool refractory_ = false;
    std::optional<std::size_t> held_;
};

}  // namespace s3::input::gesture
