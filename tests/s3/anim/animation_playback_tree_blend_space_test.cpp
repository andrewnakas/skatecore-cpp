// Port of the #[cfg(test)] module in skate-core/src/animation/playback_tree/blend_space.rs.
#include <string>

#include "harness.hpp"
#include "s3/core/animation/playback_clip.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"

using namespace s3::animation::playback_tree;
using namespace s3::animation::playback_tree::blend_space;
using s3::animation::playback_clip::PlaybackClip;
using s3::animation::skeleton_input::name::encode;

namespace {
Simplex triangle() {
    return Simplex{{0, 1, 2}, {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}}, {{-1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}},
                   {1.0f, 1.0f, 1.0f}};
}
}  // namespace

S3_TEST(bump_space_planes_and_original_outside_projection) {
    const Simplex s = triangle();
    CHECK_EQ(s.coordinates(std::vector<float>{0.25f, 0.5f}), (std::vector<float>{0.25f, 0.25f, 0.5f}));
    CHECK_EQ(s.project(std::vector<float>{2.0f, 0.5f}), (std::vector<float>{1.0f, 0.0f}));
    CHECK_EQ(s.project(std::vector<float>{0.5f, 2.0f}), (std::vector<float>{0.5f, 0.5f}));
    CHECK_EQ(s.project(std::vector<float>{-1.0f, -1.0f}), (std::vector<float>{0.0f, 0.0f}));
}

S3_TEST(bump_space_clocks_share_phase_and_keep_all_weighted_poses) {
    std::vector<PlaybackTree> children;
    for (int i = 1; i <= 3; ++i) {
        children.push_back(PlaybackTree::Clip{"CLIP" + std::to_string(i),
                                              PlaybackClip::new_(float(i * 30 + 1), 30.0f, 1.0f, 0, {})});
    }
    const auto x = encode("X");
    const auto y = encode("Y");
    auto tree = BlendSpace::new_({x, y}, children, {triangle()}).unwrap();
    tree.set_attributes(std::vector<SettableAttribute>{SettableAttribute{x, 0.25f, false, -1},
                                                       SettableAttribute{y, 0.5f, false, -1}})
        .unwrap();
    const float length = tree.length();
    tree.set_time(length * 0.2f);
    AdvanceResult property{false, -1.0f, -1.0f};
    tree.advance(length * 0.1f, 0.0f, property);
    for (const auto& child : tree.children) {
        CHECK(s3::rs::abs(child.time() / child.length() - 0.3f) < 1e-6f);
    }
    std::vector<PoseCommand> commands;
    tree.evaluate(Evaluation{0.01f, true}, true, commands).unwrap();
    CHECK_EQ(commands.size(), std::size_t(4));
    CHECK_EQ(commands.back(), PoseCommand(PoseCommand::WeightedBlend{{0.25f, 0.25f, 0.5f}}));
}
