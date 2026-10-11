// Port of the inline tests in skate-core/src/player/state.rs.
#include <algorithm>

#include "harness.hpp"
#include "s3/core/player/state.hpp"

namespace {
using namespace s3::player::state;

S3_TEST(every_native_state_round_trips_and_has_a_unique_owner_slot) {
    auto slots = s3::rs::map(ALL, [](PhysicalStateId s) { return native_owner_offset(s); });
    std::sort(slots.begin(), slots.end());
    for (std::size_t i = 0; i + 1 < slots.size(); ++i) CHECK(slots[i] != slots[i + 1]);
    for (PhysicalStateId state : ALL) {
        CHECK(try_from(static_cast<std::uint32_t>(state)) ==
              (s3::rs::Result<PhysicalStateId, UnknownPhysicalState>(state)));
        CHECK_EQ(category(state), (static_cast<std::uint32_t>(state) / 100) * 100);
    }
}

S3_TEST(only_the_six_native_grind_states_form_the_grind_range) {
    for (PhysicalStateId state : ALL) {
        const auto v = static_cast<std::uint32_t>(state);
        CHECK_EQ(is_grind(state), v >= 400 && v <= 405);
    }
}

S3_TEST(unknown_state_ids_are_rejected_without_a_fallback) {
    using R = s3::rs::Result<PhysicalStateId, UnknownPhysicalState>;
    CHECK(try_from(0) == R(s3::rs::make_err(UnknownPhysicalState{0})));
    CHECK(try_from(203) == R(s3::rs::make_err(UnknownPhysicalState{203})));
}
}  // namespace
