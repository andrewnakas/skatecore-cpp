// Port of skate-core/src/input/anticipation_intentions.rs (#[cfg(test)] mod tests).
#include <cmath>

#include "harness.hpp"
#include "s3/core/input/anticipation_intentions.hpp"

namespace {
using namespace s3::input::anticipation_intentions;
using s3::rs::to_bits;

S3_TEST(neutral_removes_anticipation_and_tail_hold_selects_ollie_range) {
    CHECK(produce(DerivedControllerInput::from_words({})).empty());
    std::array<std::uint32_t, 26> words{};
    words[10] = to_bits(-1.0f);
    const auto intents = produce(DerivedControllerInput::from_words(words));
    CHECK(std::fabs(intents[0].value - 1.0f) < 0.000001f);
    CHECK(std::fabs(intents[1].value) <= 0.52f);
    words[10] = to_bits(1.0f);
    CHECK(std::fabs(produce(DerivedControllerInput::from_words(words))[1].value) >= 2.62f);
}
}  // namespace
