// Port of skate-core/src/air/ground_normal.rs (whole file).
// TU3 Reckoning::UpdateGroundNormal 82D8E0F8, including filter 82D7A018.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"

namespace s3::air::ground_normal {

/// Native reckoning +1280..1376: control, position, previous input, filtered
/// error, output delta and filtered input delta. All four vector lanes persist.
/// Initial values must come from the native constructor/entry path.
class GroundNormalFilter {
public:
    /// TU3 constructor82D8DA68 initializes current/last target to the seed,
    /// and all three accumulated difference vectors to zero.
    static GroundNormalFilter initialized(std::array<float, 4> control, std::array<float, 4> initial) {
        GroundNormalFilter f;
        for (std::size_t i = 0; i < 4; ++i) {
            f.words_[i] = rs::to_bits(control[i]);
            f.words_[4 + i] = rs::to_bits(initial[i]);
            f.words_[8 + i] = rs::to_bits(initial[i]);
        }
        return f;
    }

    /// The up-vector filters publish the final corrected up vector only after
    /// both filter updates and acceleration/damping in82D8C8F0.
    void publish_current(std::array<float, 4> current) {
        for (std::size_t i = 0; i < 4; ++i) words_[4 + i] = rs::to_bits(current[i]);
    }

    std::array<float, 4> filter_raw(std::array<float, 4> control, std::array<float, 4> input) {
        for (std::size_t i = 0; i < 4; ++i) words_[i] = rs::to_bits(control[i]);
        return filter(input);
    }

    static GroundNormalFilter from_words(std::array<std::uint32_t, 24> words) {
        GroundNormalFilter f;
        f.words_ = words;
        return f;
    }

    const std::array<std::uint32_t, 24>& words() const { return words_; }

    /// Copies live layout+1216 control words before each native filter call.
    /// Returns the normal published at reckoning+1216 and +1296. There is no
    /// invented zero-vector fallback; native degenerate arithmetic is preserved.
    std::array<float, 4> update(std::array<float, 4> control, std::array<float, 4> input) {
        for (std::size_t i = 0; i < 4; ++i) words_[i] = rs::to_bits(control[i]);
        const std::array<float, 4> filtered = filter(input);
        const float squared = physics::native_arithmetic::dot3(filtered, filtered);
        float inverse = physics::reciprocal_sqrt::estimate(squared);
        for (int i = 0; i < 2; ++i) {
            const float correction = rs::mul_add(-squared, inverse * inverse, 1.0f);
            inverse = rs::mul_add(inverse * 0.5f, correction, inverse);
        }
        std::array<float, 4> normal{};
        for (std::size_t i = 0; i < 4; ++i) normal[i] = filtered[i] * inverse;
        for (std::size_t i = 0; i < 4; ++i) words_[4 + i] = rs::to_bits(normal[i]);
        return normal;
    }

    // Full vector filter 82D7A018, with publication order before normalization.
    // pub(crate) in Rust.
    std::array<float, 4> filter(std::array<float, 4> input) {
        const std::array<float, 24> previous = rs::from_bits(words_);
        const float blend = previous[3];
        const float retained = 1.0f - blend;
        std::array<float, 4> output{};
        for (std::size_t lane = 0; lane < 4; ++lane) {
            const float error = input[lane] - previous[4 + lane];
            const float error_blend = rs::mul_add(previous[12 + lane], retained, error * blend);
            const float input_delta = input[lane] - previous[8 + lane];
            const float input_blend = rs::mul_add(previous[20 + lane], retained, input_delta * blend);
            float predicted = rs::mul_add(error_blend, previous[1], previous[4 + lane]);
            predicted = rs::mul_add(error, previous[0], predicted);
            output[lane] = rs::mul_add(input_blend - previous[16 + lane], previous[2], predicted);
            words_[12 + lane] = rs::to_bits(error_blend);
            words_[20 + lane] = rs::to_bits(input_blend);
            words_[16 + lane] = rs::to_bits(output[lane] - previous[4 + lane]);
            words_[4 + lane] = rs::to_bits(output[lane]);
            words_[8 + lane] = rs::to_bits(input[lane]);
        }
        return output;
    }

    bool operator==(const GroundNormalFilter& o) const { return words_ == o.words_; }
    bool operator!=(const GroundNormalFilter& o) const { return !(*this == o); }

private:
    std::array<std::uint32_t, 24> words_{};
};

}  // namespace s3::air::ground_normal
