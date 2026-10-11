// Port of skate-game/src/difficulty.rs #[cfg(test)] mod tests.
#include "harness.hpp"
#include "s3/game/difficulty.hpp"

using namespace s3::game::difficulty;

S3_TEST(retail_indices_and_saved_names_agree) {
    std::size_t index = 0;
    for (Difficulty d : ALL) {
        CHECK_EQ(static_cast<std::size_t>(d), index);
        CHECK(parse(label(d)).unwrap() == d);
        const std::string bytes = saved_json(d);
        CHECK(parse_saved(bytes).unwrap() == d);
        ++index;
    }
    CHECK(parse("motorized").is_err());
    CHECK(parse_saved(R"({"difficulty":"made-up"})").is_err());
}
