// Port of skate-core/src/player/offboard/reset_animation/tests.rs.
#include <tuple>

#include "harness.hpp"
#include "s3/core/player/offboard/reset_animation.hpp"

namespace {
using namespace s3::player::offboard::reset_animation;

S3_TEST(reset_is_selective_not_full_default) {
    std::array<std::uint32_t, 21> words;
    words.fill(UINT32_MAX);
    std::uint32_t gesture = 8;
    std::array<std::uint8_t, 4> bytes{9, 9, 9, 9};
    motion_output(words, gesture, bytes);
    CHECK_EQ(words[18], UINT32_MAX);
    CHECK_EQ(words[19], UINT32_MAX);
    CHECK_EQ(words[11], 0x7fffffffu);
    CHECK_EQ(words[20], 0x03fbffffu);
    CHECK_EQ(gesture, 37u);
    CHECK_EQ(bytes, (std::array<std::uint8_t, 4>{0, 0, 9, 9}));
}

S3_TEST(given_stance_retains_lower_flags_and_consumes_request) {
    const std::tuple<std::uint32_t, std::uint32_t, bool, std::uint32_t> cases[] = {
        {0, 0, false, 3}, {0, 1, true, 0}, {1, 0, false, 0}, {1, 1, true, 3}, {2, 0, true, 3}};
    for (const auto& [original, req, mirror, upper] : cases) {
        std::uint32_t request = req;
        std::uint32_t flags = 0x1234;
        bool mirrored = false;
        given_stance(original, request, flags, mirrored);
        CHECK_EQ(mirrored, mirror);
        CHECK_EQ(flags, (upper << 30) | 0x1234u);
        CHECK_EQ(request, 0u);
    }
}

S3_TEST(playback_reset_preserves_unrelated_flags_and_resets_only_meaningful_request_words) {
    std::uint32_t flags = UINT32_MAX;
    bool mirrored = true;
    std::uint32_t word_15320 = 17;
    skater_flags(flags, mirrored, word_15320);
    CHECK_EQ(flags, 0x0ff7ffffu);
    CHECK(!mirrored);
    CHECK_EQ(word_15320, 0u);
    // The local-player bit controls later animation culling and must survive.
    CHECK_NE(flags & 0x08000000u, 0u);
    std::array<std::uint32_t, 4> request;
    request.fill(UINT32_MAX);
    bool active = true;
    motion_request(request, active);
    CHECK_EQ(request, (std::array<std::uint32_t, 4>{0, 0x3e4ccccdu, 0, 0}));
    CHECK(!active);
}
}  // namespace
