// Port of skate-game/src/graph_host/tests/pushing_metadata.rs.
#include <cmath>
#include <iostream>

#include "assets.hpp"
#include "harness.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/graph_host/pushing_settings.hpp"
#include "s3/game/graph_runtime/disk.hpp"

using namespace s3::game::graph_host::pushing_settings;

S3_TEST(stock_push_metrics_load_through_the_runtime_data_path) {
    S3_REQUIRE_ASSETS(root);
    const s3::data::Vfs vfs = s3::game::graph_runtime::disk::read_files(root, {s3::game::graph_runtime::disk::COLLECTIONS});
    auto collections = s3::data::Collections::load(vfs, root);
    if (!collections.ok) s3test::fail(__FILE__, __LINE__, collections.error);
    const PushingSettings settings = PushingSettings::load_stock(collections.value, root).unwrap();
    for (const PushAttributes* group : {&settings.regular, &settings.mongo}) {
        for (const auto& clip : group->clips) {
            CHECK(std::isfinite(clip.length) && clip.length > 0.0f);
            CHECK(clip.end_velocity > clip.begin_velocity);
        }
    }
    // Distinct authored timings must survive data conversion and dispatch.
    CHECK_NE(settings.regular.clips[0].length, settings.mongo.clips[0].length);
    std::cout << "regular.clips[0].length=" << settings.regular.clips[0].length
              << " mongo.clips[0].length=" << settings.mongo.clips[0].length << "\n";
}
