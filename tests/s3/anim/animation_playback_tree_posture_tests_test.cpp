// Port of skate-core/src/animation/posture/tree_tests.rs (Rust module
// animation::playback_tree::posture_tests).
#include <memory>

#include "harness.hpp"
#include "s3/core/animation/playback_attributes.hpp"
#include "s3/core/animation/playback_clip.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/animation/posture.hpp"

using namespace s3::animation::playback_tree;
using s3::animation::playback_attributes::AttributeMirror;
using s3::animation::playback_clip::PlaybackClip;
using s3::animation::posture::PosturePose;

S3_TEST(posture_commands_stay_inside_bind_and_mirror_layers) {
    const std::optional<PosturePose> postures[] = {std::nullopt, PosturePose::Stiff, PosturePose::Slouch,
                                                   PosturePose::Buff};
    for (const auto& posture : postures) {
        PlaybackTree tree = PlaybackTree::BindPose{
            PlaybackTree(PlaybackTree::Clip{"motion", PlaybackClip::new_(31.0f, 30.0f, 1.0f, 0, {})}),
            posture,
            true,
            {2, 1},
            std::make_shared<const AttributeMirror>(AttributeMirror{{}}),
        };
        tree.set_time(0.25f);
        CHECK_EQ(tree.time(), 0.25f);
        CHECK_EQ(tree.length(), 1.0f);
        const Evaluation parameters{0.0f, false};
        std::vector<PoseCommand> commands;
        CHECK(!tree.evaluate(parameters, false, commands).unwrap());
        CHECK(commands.empty());
        //Cloning preserves the inner posture selection through tree ownership changes.
        CHECK(PlaybackTree(tree).evaluate(parameters, true, commands).unwrap());
        const PoseCommand first = commands.front();
        commands.erase(commands.begin());
        CHECK(std::holds_alternative<PoseCommand::Clip>(first.value));
        std::vector<PoseCommand> expected;
        if (posture) {
            expected.push_back(PoseCommand::Pose{s3::animation::posture::posture_pose_name(*posture)});
            expected.push_back(PoseCommand::Add{true});
        }
        for (PoseCommand c : {PoseCommand(PoseCommand::Pose{"RIG_TPOSE"}), PoseCommand(PoseCommand::Add{true}),
                              PoseCommand(PoseCommand::Pose{"BOARD_BACKWARDS"}), PoseCommand(PoseCommand::Add{false}),
                              PoseCommand(PoseCommand::Pose{"BOARD_BACKWARDS_IK"}), PoseCommand(PoseCommand::Add{true}),
                              PoseCommand(PoseCommand::Mirror{2}), PoseCommand(PoseCommand::Mirror{1})}) {
            expected.push_back(c);
        }
        CHECK_EQ(commands, expected);
    }
}
