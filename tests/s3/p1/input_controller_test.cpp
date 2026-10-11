// Port of skate-core/src/input/controller_startup.rs.
#include "harness.hpp"
#include "s3/core/input/controller.hpp"

namespace {
using namespace s3::input::controller;

S3_TEST(initialize_preserves_only_native_low_twenty_flag_bits) {
    std::array<std::uint32_t, 26> words{};
    words[6] = 0xfff01234u;
    words[13] = 0xabcd5678u;
    words[0] = 1;
    words[24] = 2;
    auto input = DerivedControllerInput::from_words(words);
    input.initialize();
    CHECK_EQ(input.words()[6], 0x00001234u);
    CHECK_EQ(input.words()[13], 0x000d5678u);
    for (std::size_t index = 0; index < 26; ++index) {
        if (!(index == 6 || index == 13 || (index >= 16 && index < 20))) {
            CHECK_MSG(input.words()[index] == 0, "word " << index);
        }
    }
    for (std::size_t index = 16; index < 20; ++index) {
        CHECK_EQ(input.words()[index], 0x7effffffu);
    }
}
}  // namespace
