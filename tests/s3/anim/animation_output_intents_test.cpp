// Port of skate-core/src/animation/output/tests/intents.rs.
#include "harness.hpp"
#include "s3/core/animation/output/intents.hpp"

using namespace s3::animation::output::intents;
using s3::input::graph_intents::IntentMutation;

namespace {
IntentName key(std::uint32_t seed) { return IntentName{{seed, 0, 0, 0, 0, 0}}; }
}  // namespace

S3_TEST(set_overwrites_and_remove_preserves_other_chain_entries) {
    IntentMap map{};
    const IntentName first = key(1);
    const IntentName second = key(1 + static_cast<std::uint32_t>(BUCKET_COUNT));
    CHECK(map.insert(Intent{first, 1.0f}).unwrap());
    CHECK(map.insert(Intent{second, 2.0f}).unwrap());
    CHECK(!map.set(first, 7.0f).unwrap());
    {
        const auto entries = map.entries();
        const Intent* found = nullptr;
        for (const auto& entry : entries) {
            if (entry.name == first) {
                found = &entry;
                break;
            }
        }
        CHECK(found != nullptr);
        CHECK_EQ(found->value, 7.0f);
    }
    CHECK(map.remove(first));
    CHECK(!map.remove(first));
    CHECK_EQ(map.len(), std::size_t(1));
    CHECK(map.entries().front().name == second);
}

S3_TEST(apply_uses_existing_mutation_protocol_and_capacity) {
    IntentMap map{};
    const IntentName name = key(42);
    CHECK(map.apply(name, IntentMutation::Set{3.0f}).unwrap());
    CHECK(!map.apply(name, IntentMutation::None{}).unwrap());
    CHECK(!map.apply(name, IntentMutation::Set{4.0f}).unwrap());
    CHECK_EQ(map.entries().front().value, 4.0f);
    CHECK(map.apply(name, IntentMutation::Remove{}).unwrap());
    CHECK_EQ(map.len(), std::size_t(0));
}

S3_TEST(insert_rejects_only_after_native_fixed_capacity) {
    IntentMap map{};
    for (std::uint32_t seed = 0; seed < static_cast<std::uint32_t>(ENTRY_CAPACITY); ++seed) {
        CHECK(map.insert(Intent{key(seed), static_cast<float>(seed)}).is_ok());
    }
    CHECK(map.insert(Intent{key(static_cast<std::uint32_t>(ENTRY_CAPACITY)), 0.0f}).is_err());
}
