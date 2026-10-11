// Shared loading steps of the stock MotionHost tests (skate-game/src/graph_host/tests/motion_*.rs).
// Each Rust test repeats: StateGraph::load(MotionGraph_OnBoard) -> Binding::from_graph ->
// CompiledGraph::from_binding, Collections::load(root), AnimationBanks::load(root).metadata(),
// MotionHost::from_graph(...).unwrap(). The `.unwrap()`s become test failures here.
#pragma once
#include <string>

#include "harness.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/graph_host/motion.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/graph_runtime/disk.hpp"

namespace s3test::motion_fixture {

inline constexpr const char* MOTION_GRAPH = "private/stock/data/state/MotionGraph_OnBoard.stategraph";

inline s3::game::graph_runtime::LoadedGraph stock_graph(const std::string& root) {
    const s3::data::Vfs vfs = s3::game::graph_runtime::disk::read_files(root, {MOTION_GRAPH});
    return s3::game::graph_runtime::load_graph(vfs, root, MOTION_GRAPH).unwrap();
}

inline s3::data::Collections collections(const std::string& root) {
    const s3::data::Vfs vfs =
        s3::game::graph_runtime::disk::read_files(root, {s3::game::graph_runtime::disk::COLLECTIONS});
    auto loaded = s3::data::Collections::load(vfs, root);
    if (!loaded.ok) s3test::fail(__FILE__, __LINE__, loaded.error);
    return std::move(loaded.value);
}

inline s3::data::AnimationMetadata metadata(const std::string& root) {
    const s3::data::Vfs vfs = s3::game::graph_runtime::disk::read_banks(root);
    auto banks = s3::data::AnimationBanks::load(vfs, root);
    if (!banks.ok) s3test::fail(__FILE__, __LINE__, banks.error);
    auto loaded = banks.value.metadata();
    if (!loaded.ok) s3test::fail(__FILE__, __LINE__, loaded.error);
    return std::move(loaded.value);
}

inline s3::animation::playback::PlaybackContext context(std::optional<bool> is_switch, std::optional<bool> is_mirrored,
                                                        std::optional<bool> board_available) {
    s3::animation::playback::PlaybackContext c{};
    c.is_switch = is_switch;
    c.is_mirrored = is_mirrored;
    c.board_available = board_available;
    c.pro_skater = s3::animation::skeleton_input::name::encode("");
    c.transition_override = std::nullopt;
    return c;
}

inline s3::game::graph_host::motion::MotionHost host(const std::string& root,
                                                     const s3::game::graph_runtime::LoadedGraph& graph,
                                                     s3::animation::playback::PlaybackContext playback_context) {
    auto created = s3::game::graph_host::motion::MotionHost::from_graph(graph, collections(root), metadata(root),
                                                                        std::move(playback_context));
    if (created.is_err()) s3test::fail(__FILE__, __LINE__, created.unwrap_err());
    return std::move(created.unwrap());
}

inline s3::graph::controller::Frame frame(float dt) {
    s3::graph::controller::Frame f;
    f.dt = dt;
    f.current = std::nullopt;
    f.last = std::nullopt;
    f.state_times = {};
    return f;
}

}  // namespace s3test::motion_fixture
