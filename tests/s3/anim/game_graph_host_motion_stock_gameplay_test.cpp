// Port of skate-game/src/graph_host/motion_stock_gameplay.rs (#[cfg(test)] mod tests).
#include "harness.hpp"
#include "s3/game/graph_host/motion_stock_gameplay.hpp"

using namespace s3::game::graph_host::motion_stock_gameplay;

S3_TEST(moving_object_paths_use_stock_separator_normalization) {
    MovingObjectRegistry registry;
    registry.register_("ram:/world/objects/car");
    registry.begin("ram:\\world\\objects\\car").unwrap();
    CHECK(registry.active());
    CHECK(registry.update("ram:/world/objects/car").unwrap());
    registry.end();
    CHECK(!registry.active());
}

S3_TEST(unknown_moving_object_is_an_explicit_producer_error) {
    MovingObjectRegistry registry;
    const std::string error = registry.begin("ram:/missing").unwrap_err();
    CHECK(error.find("not registered") != std::string::npos);
}
