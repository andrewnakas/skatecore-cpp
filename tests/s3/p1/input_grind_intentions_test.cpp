// Port of skate-core/src/input/grind_intentions/tests.rs.
#include <string_view>
#include <utility>
#include <vector>

#include "harness.hpp"
#include "s3/core/input/grind_intentions.hpp"

namespace {
using namespace s3::input::grind_intentions;
using s3::rs::to_bits;
using Pair = std::pair<std::string_view, float>;

std::vector<Pair> values(float x, float rx, float ry) {
    std::array<std::uint32_t, 26> words{};
    words[7] = to_bits(x);
    words[9] = to_bits(rx);
    words[10] = to_bits(ry);
    std::vector<Pair> out;
    for (const auto& i : produce(DerivedControllerInput::from_words(words))) out.emplace_back(i.name, i.value);
    return out;
}
bool same(const std::vector<Pair>& a, const std::vector<Pair>& b) { return a == b; }

S3_TEST(all_four_s3_emissions_preserve_order_and_sign) {
    CHECK(same(values(0.25f, 0.5f, -0.75f), {
                                                {"GrindBalanceX", -0.25f},
                                                {"PhysGrindTranslation", 0.75f},
                                                {"PhysGrindStabilityNudge", 0.25f},
                                                {"PhysGrindUpDown", -0.75f},
                                            }));
}
S3_TEST(translation_saturates_both_directions_and_cancellation_omits_only_it) {
    CHECK(values(0.75f, 0.75f, 0.0f)[1] == Pair("PhysGrindTranslation", 1.0f));
    CHECK(values(-0.75f, -0.75f, 0.0f)[1] == Pair("PhysGrindTranslation", -1.0f));
    CHECK(same(values(0.5f, -0.5f, 0.0f), {{"GrindBalanceX", -0.5f}, {"PhysGrindStabilityNudge", 0.5f}}));
    CHECK(values(0.0f, -0.0f, 0.0f).empty());
}
S3_TEST(left_only_translation_distinguishes_s3_from_s2) {
    CHECK(values(0.5f, 0.0f, 0.0f)[1] == Pair("PhysGrindTranslation", 0.5f));
}
}  // namespace
