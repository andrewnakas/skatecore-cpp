// Port of skate-game/src/physics/biped_ground/grab_runtime/tests.rs
#include <memory>

#include "harness.hpp"
#include "s3/game/physics/biped_ground/grab_runtime.hpp"

using namespace s3::game::physics::biped_ground::grab_runtime;
using s3::player::offboard::grab_scene::Geometry;

namespace {
Record record(std::uint32_t id, float y, float z) {
    const Vector a{-1.0f, y, z, 0.0f};
    const Vector b{1.0f, y, z, 0.0f};
    auto geometry = std::make_shared<Geometry>();
    geometry->id = id;
    geometry->points = {a, b};
    geometry->approach_vectors = {};
    geometry->word_60 = 0;
    Record r{};
    r._1 = geometry;
    r._0[47] = 2;
    r._0[48] = id;
    r._0[49] = id;
    r._0[52] = id;
    r.set_vector(64, a);
    r.set_vector(80, b);
    return r;
}
}  // namespace

S3_TEST(s3_validation_reprocesses_swap_with_next_hit_not_next_triplet) {
    std::vector<Record> records{record(1, 0.0f, 1.0f), record(2, 0.0f, 1.0f), record(3, 0.0f, 1.0f)};
    //First null-assembly obstruction removes1. Next hit belongs to3: if the
    //implementation aligns to three hits or keeps original order, it rejects3.
    const std::vector<std::optional<Hit>> hits{
        Hit{0.5f, std::nullopt}, Hit{0.5f, 3u}, std::nullopt, std::nullopt, Hit{0.5f, 2u},
        std::nullopt,            std::nullopt,  std::nullopt, std::nullopt,
    };
    validate(records, hits);
    std::vector<std::uint32_t> ids;
    for (const auto& r : records) ids.push_back(r.descriptor().id);
    CHECK((ids == std::vector<std::uint32_t>{3, 2}));
}

S3_TEST(nearest_ineligible_suppresses_farther_eligible) {
    const auto near_ = record(1, 0.6f, 0.1f);
    const auto far_ = record(2, 0.0f, 2.0f);
    CHECK(selection::best({far_}, {0.0f, 0.0f, 0.0f, 0.0f}).has_value());
    CHECK(!selection::best({far_, near_}, {0.0f, 0.0f, 0.0f, 0.0f}).has_value());
}

S3_TEST(both_height_boundaries_are_exclusive) {
    for (float height : {-0.6f, 0.6f}) {
        CHECK(!selection::best({record(1, height, 0.2f)}, {0.0f, 0.0f, 0.0f, 0.0f}).has_value());
    }
}

S3_TEST(invalidation_preserves_retained_candidates_and_interactable_completion) {
    Owner owner{};
    owner.validated.push_back(record(1, 0.0f, 1.0f));
    owner.flags_12836 = 0xff;
    owner.interactable_result = std::optional<std::uint32_t>(44u);
    owner.data_ready = {true, true};
    owner.invalidate();
    CHECK_EQ(int(owner.flags_12836), 0x3f);
    CHECK_EQ(owner.validated.size(), std::size_t(1));
    CHECK((owner.interactable_result == std::optional<std::optional<std::uint32_t>>(44u)));
    CHECK((owner.data_ready == std::array<bool, 2>{false, false}));
    owner.enter_reset();
    CHECK(!owner.interactable_result.has_value());
}

S3_TEST(publication_consumes_ready_not_retained_data) {
    Owner owner{};
    owner.data[0] = record(1, 0.0f, 1.0f);
    owner.data_ready[0] = true;
    owner.interactable_result = std::optional<std::uint32_t>(55u);
    const auto first = owner.publish();
    CHECK(first.records[0].has_value());
    CHECK((first.object == std::optional<std::optional<std::uint32_t>>(55u)));
    CHECK(owner.interactable_latched);
    const auto second = owner.publish();
    CHECK(!second.records[0].has_value());
    CHECK(!second.object.has_value());
    CHECK(owner.data[0].has_value());
}
