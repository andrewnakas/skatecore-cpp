// Port of skate-game/src/tests/animation_pose.rs.
#include <cmath>

#include "assets.hpp"
#include "harness.hpp"
#include "s3/core/animation/pose_add.hpp"
#include "s3/game/animation_pose.hpp"

using namespace s3::game::animation_pose;
namespace pose_add = s3::animation::pose_add;
namespace rs = s3::rs;

namespace {
bool all_finite(const std::vector<Sqt>& pose) {
    for (const Sqt& s : pose) {
        for (float v : s.scale)
            if (!std::isfinite(v)) return false;
        for (float v : s.rotation)
            if (!std::isfinite(v)) return false;
        for (float v : s.translation)
            if (!std::isfinite(v)) return false;
    }
    return true;
}
}  // namespace

S3_TEST(decoded_clip_interpolates_across_compression_boundaries) {
    // Uniform motion makes a skipped key or a held pose unambiguous. Exercise
    // both full-pose sampling and the trajectory's previous-frame sampler.
    for (float fps : {30.0f, 60.0f}) {
        ClipFrames clip;
        clip.name = "BOUNDARY_RAMP";
        clip.source_offset = 0;
        clip.fps_bits = rs::to_bits(fps);
        clip.loop_translation_bits = {0, 0, 0};
        clip.loop_rotation_bits = {0, 0, 0, rs::to_bits(1.0f)};
        clip.channel_animation = true;
        clip.channel_weights = {rs::to_bits(0.25f)};
        for (int frame = 0; frame <= 17; ++frame) {
            const float half_angle = static_cast<float>(frame) * 0.02f;
            clip.frames.push_back({rs::to_bits(std::array<float, 10>{1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
                                                                      rs::sin(half_angle), rs::cos(half_angle),
                                                                      static_cast<float>(frame), 0.0f, 0.0f})});
        }
        for (int quarter = 0; quarter <= 68; ++quarter) {
            const float frame = static_cast<float>(quarter) * 0.25f;
            const float time = frame / fps;
            const Sqt pose = sample_clip(clip, time).unwrap()[0];
            CHECK_EQ(pose, sample_bone(clip, time, 0).unwrap());
            CHECK_MSG(rs::abs(pose.translation[0] - frame) < 0.0001f,
                      fps << " fps, frame " << frame << ": skipped/held decoded key");
            const float angle = 2.0f * rs::atan2(pose.rotation[2], pose.rotation[3]);
            CHECK(rs::abs(angle - frame * 0.04f) < 0.0001f);
            CHECK_EQ(pose.translation[3], 0.25f);
        }
    }
}

S3_TEST(stock_pose_commands_sample_complete_bones_and_preserve_tree_order) {
    S3_REQUIRE_ASSETS(root);
    auto loaded = PoseEvaluator::load(root);
    const PoseEvaluator evaluator = std::move(loaded.unwrap());
    CHECK_EQ(evaluator.frames.parents.size(), std::size_t(36));
    CHECK_EQ(evaluator.frames.clip_count(), std::size_t(2672 + 652));
    for (const std::string& name : evaluator.frames.clip_names()) {
        const ClipFrames& clip = *evaluator.frames.clip(name).value;
        const float end = static_cast<float>(clip.frames.size() - 1) / rs::from_bits(clip.fps_bits);
        const std::vector<PoseCommand> commands = {
            PoseCommand::Clip{clip.name, 0.0f, 0.0f, 0},
            PoseCommand::Clip{clip.name, 0.0f, end * 0.5f, 0},
            PoseCommand::Blend{0.375f},
            PoseCommand::Clip{clip.name, end * 0.5f, end, 1},
            PoseCommand::ChannelBlend{0.25f, false},
        };
        const std::vector<Sqt> pose = evaluator.evaluate(commands).unwrap();
        CHECK_MSG(pose.size() == 36, clip.name);
        CHECK_MSG(all_finite(pose), clip.name);
        // Regular stance takes every native AddBindPose branch. Include the
        // trajectory-relative mode used by the source's one-shot mirror flag.
        std::vector<PoseCommand> wrapped = commands;
        wrapped.push_back(PoseCommand::Pose{"RIG_TPOSE"});
        wrapped.push_back(PoseCommand::Add{true});
        wrapped.push_back(PoseCommand::Pose{"BOARD_BACKWARDS"});
        wrapped.push_back(PoseCommand::Add{false});
        wrapped.push_back(PoseCommand::Pose{"BOARD_BACKWARDS_IK"});
        wrapped.push_back(PoseCommand::Add{true});
        wrapped.push_back(PoseCommand::Mirror{2});
        wrapped.push_back(PoseCommand::Mirror{1});
        const std::vector<Sqt> wrapped_pose = evaluator.evaluate(wrapped).unwrap();
        CHECK_MSG(all_finite(wrapped_pose), clip.name);
        const std::vector<NativeMatrix> hierarchy = evaluator.hierarchy(wrapped_pose).unwrap();
        bool finite = true;
        for (const auto& m : hierarchy)
            for (const auto& row : m)
                for (float v : row) finite = finite && std::isfinite(v);
        CHECK_MSG(finite, clip.name);
    }
    // Native SetDBContent82D1B45C overwrites equal keys; last RIG_TPOSE wins.
    CHECK_EQ(evaluator.frames.reference_pose(0xA46D20).value->samples.size(), std::size_t(36));
    CHECK_EQ(evaluator.frames.named_pose("RIG_TPOSE").value->source_offset, std::uint64_t(0xA46D20));
    CHECK_EQ(evaluator.frames.named_pose("BOARD_BACKWARDS").value->source_offset, std::uint64_t(0xA473D0));
    CHECK_EQ(evaluator.frames.bone_names[0], std::string("TRAJECTORY"));
    CHECK_EQ(evaluator.frames.bone_names[25], std::string("SKATEBOARD_ROOT"));
}

S3_TEST(additive_pose_keeps_motion_channel_and_reference_rotation_order) {
    Sqt motion;
    motion.scale = {1.0f, 1.0f, 1.0f, 1.0f};
    motion.rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    motion.translation = {2.0f, 3.0f, 4.0f, 0.25f};
    Sqt reference;
    reference.scale = {2.0f, 2.0f, 2.0f, 2.0f};
    reference.rotation = {0.0f, 1.0f, 0.0f, 0.0f};
    reference.translation = {7.0f, 11.0f, 13.0f, 0.75f};
    const Sqt result = pose_add::add(motion, reference, true);
    CHECK(result.rotation == (std::array<float, 4>{0.0f, 0.0f, -1.0f, 0.0f}));
    CHECK(result.translation == (std::array<float, 4>{5.0f, 14.0f, 9.0f, 0.25f}));
    CHECK_EQ(pose_add::add(motion, reference, false).translation[3], 0.75f);
}
