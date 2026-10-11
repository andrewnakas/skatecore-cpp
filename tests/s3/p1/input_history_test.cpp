// Port of skate-core/src/input/history_tests.rs.
#include "harness.hpp"
#include "s3/core/input/history.hpp"

namespace {
using namespace s3::input::history;
using s3::rs::to_bits;

std::array<Pad, DEVICE_SLOTS> pads() { return {Pad::new_(), Pad::new_(), Pad::new_(), Pad::new_()}; }
std::array<HistoryRecord, DEVICE_SLOTS> batch(float seed) {
    return {
        HistoryRecord::new_({seed}),
        HistoryRecord::new_({seed + 1.0f}),
        HistoryRecord::new_({seed + 2.0f}),
        HistoryRecord::new_({seed + 3.0f}),
    };
}

S3_TEST(drains_newest_batch_once_for_all_devices) {
    auto history = PadHistory::new_();
    history.publish(batch(10.0f));
    history.publish(batch(20.0f));
    auto output = pads();
    CHECK(history.drain_to_latest(output));
    const float expected[4] = {20.0f, 21.0f, 22.0f, 23.0f};
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK_EQ(output[i].count(), std::size_t(1));
        CHECK_EQ(output[i].records()[0][0], to_bits(expected[i]));
        // only the final batch advances Pad
        CHECK_EQ(output[i].records()[0][3], 1u);
    }
    CHECK(!history.drain_to_latest(output));
}

S3_TEST(producer_wraps_at_full_lap_without_overflow_policy) {
    auto history = PadHistory::new_();
    for (std::size_t i = 0; i < HISTORY_CAPACITY - 1; ++i) {
        history.publish(batch(float(i)));
    }
    CHECK_EQ(history.write_index(), std::size_t(0));
    auto output = pads();
    CHECK(history.drain_to_latest(output));
    history.publish(batch(99.0f));
    CHECK_EQ(history.write_index(), std::size_t(1));
    CHECK(history.drain_to_latest(output));
    auto full = PadHistory::new_();
    for (std::size_t i = 0; i < HISTORY_CAPACITY; ++i) {
        full.publish(batch(float(i)));
    }
    CHECK_EQ(full.write_index(), std::size_t(1));
    CHECK(!full.drain_to_latest(output));
}

S3_TEST(empty_publish_advances_once) {
    auto history = PadHistory::new_();
    history.publish(batch(1.0f));
    history.publish(std::vector<HistoryRecord>{});
    CHECK_EQ(history.write_index(), std::size_t(3));
}

S3_TEST(reused_batch_retains_unregistered_slots) {
    auto history = PadHistory::new_();
    history.batches[1] = batch(40.0f);
    auto output = pads();
    history.publish(std::vector<HistoryRecord>{HistoryRecord::new_({99.0f})});
    CHECK(history.drain_to_latest(output));
    CHECK_EQ(output[0].records()[0][0], to_bits(99.0f));
    CHECK_EQ(output[1].records()[0][0], to_bits(41.0f));
    CHECK_EQ(output[2].records()[0][0], to_bits(42.0f));
    CHECK_EQ(output[3].records()[0][0], to_bits(43.0f));
}

S3_TEST(clear_count_preserves_visibility_contract) {
    auto record = HistoryRecord::new_({3.5f, 4.5f});
    record.clear_count();
    CHECK_EQ(record.count(), std::size_t(0));
    CHECK(record.values().empty());
    CHECK_EQ(record.values_[0], 3.5f);
    CHECK_EQ(record.values_[1], 4.5f);
}
}  // namespace
