// Port of skate-core/src/animation/vbr/tests/bits.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/animation/vbr/bits.hpp"

using s3::animation::vbr::bits::unpack;

S3_TEST(native_sign_bit_and_zero_width_do_not_consume_extra_selectors) {
    // Widths3,0,2: selector bits11, packed negative5=1010 and positive2=101.
    const std::vector<std::uint8_t> block = {2, 3, 0x5a};
    const std::vector<std::uint32_t> widths = {0x0203};
    const auto values = unpack(block, widths).unwrap();
    CHECK_EQ(values[0][0], -5.0f);
    CHECK_EQ(values[0][1], 0.0f);
    CHECK_EQ(values[0][2], 2.0f);
    const std::vector<std::uint8_t> zero_block = {1, 1, 0};
    const std::vector<std::uint32_t> zero_widths = {1};
    const auto negative_zero = unpack(zero_block, zero_widths).unwrap();
    CHECK_EQ(s3::rs::to_bits(negative_zero[0][0]), 0x80000000u);
}

S3_TEST(packed_words_continue_across_the_native_vector_boundary) {
    // Nine width15 coefficients consume nine sign+magnitude16-bit words.
    std::vector<std::uint8_t> bytes = {9, 0xff, 1};
    const std::vector<std::uint8_t> tail = {0xff, 0xff, 2, 0, 5, 0, 6, 0, 9, 0, 10, 0, 13, 0, 14, 0, 17, 0};
    bytes.insert(bytes.end(), tail.begin(), tail.end());
    const std::vector<std::uint32_t> widths = {0xffffffffu, 15};
    const auto values = unpack(bytes, widths).unwrap();
    CHECK((values[0] == std::array<float, 8>{32767.0f, -1.0f, 2.0f, -3.0f, 4.0f, -5.0f, 6.0f, -7.0f}));
    CHECK_EQ(values[1][0], 8.0f);
    bytes.pop_back();
    CHECK(unpack(bytes, widths).is_err());
}
