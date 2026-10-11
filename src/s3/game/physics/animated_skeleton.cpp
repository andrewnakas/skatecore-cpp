// Port of skate-game/src/physics/animated_skeleton.rs.
#include "s3/game/physics/animated_skeleton.hpp"

#include <algorithm>

#include "s3/core/math.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/skeleton_landing_on_board.hpp"
#include "s3/data/physics_skeleton.hpp"

namespace s3::game::physics::animated_skeleton {

using sc::math::Vector3;
using sc::physics::board::BodyId;
using sc::physics::skeleton_animation_record::compose_affine;
using sc::physics::skeleton_animation_record::IDENTITY;
using sc::point_graph::PointGraph;

namespace {
bool eq_ignore_ascii_case(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}
}  // namespace

rs::Result<AnimatedSkeleton, std::string> AnimatedSkeleton::load(const sd::Vfs& vfs, const std::string& asset_root,
                                                                 const sd::Collections& data,
                                                                 const sd::AnimationFrames& animation,
                                                                 bool head_has_hat) {
    S3G_TRY(auto skeleton,
            sd::PhysicsSkeleton::load(vfs, sd::join_path(asset_root, "private/stock/physics-skeletons.json"),
                                      animation.source_sha256, "PHYS_TPOSE"));
    if (skeleton.bones.size() != 24)
        return rs::make_err(std::string("Stock skater physics skeleton must contain24 parts"));
    auto lookup = [&](const std::string& name) -> rs::Result<std::size_t, std::string> {
        for (std::size_t i = 0; i < animation.bone_names.size(); ++i)
            if (eq_ignore_ascii_case(animation.bone_names[i], name)) return i;
        return rs::make_err(fmt::cat("Stock physics bone ", name, " is absent from the animation hierarchy"));
    };
    AnimatedSkeleton out;
    std::array<Vector3, 24> sizes{};
    std::array<std::uint32_t, 24> shapes{};
    out.physics_frames.fill(IDENTITY);
    for (std::size_t part = 0; part < skeleton.bones.size(); ++part) {
        const auto& bone = skeleton.bones[part];
        S3G_TRY(out.bone_indices[part], lookup(bone.name));
        std::array<float, 4> translation{};
        for (std::size_t i = 0; i < 4; ++i) translation[i] = rs::from_bits(bone.words[16 + i]);
        out.physics_frames[part] = sc::physics::skeleton_animation_record::physics_bone_frame(bone.rotation(), translation);
        const auto size = bone.size();
        sizes[part] = Vector3{size[0], size[1], size[2]};
        S3G_TRY(auto words, data.words<6>("physics_skeleton_bones", "default", "PART_" + bone.name));
        shapes[part] = words[3];
    }
    // Native82BED688 exact order and names. These are board-parented
    // targets; do not replace them with the physical foot/hand indices.
    const char* target_names[4] = {"LeftToeBase_Reparented", "RightToeBase_Reparented", "LeftHand_Reparented",
                                   "RightHand_Reparented"};
    for (std::size_t i = 0; i < 4; ++i) {
        S3G_TRY(out.target_bones[i], lookup(target_names[i]));
    }
    out.record = sc::physics::skeleton_animation_record::SkeletonAnimationRecord{};
    out.roots = sc::physics::skeleton_root::SkeletonRootFrames{};
    out.board_frames = sc::physics::skeleton_board_frames::SkeletonBoardFrames{};
    out.board_offset = sc::physics::skeleton_board_offset::SkateboardOffset{};
    out.landing = sc::physics::skeleton_landing::LandingAdjustment{};
    out.targets.fill(IDENTITY);
    out.masses = sc::physics::skeleton_animation_record::SkeletonAnimationMasses::from_bone_data(sizes, shapes,
                                                                                                head_has_hat);
    out.animation_board = IDENTITY;
    out.unadjusted_board = IDENTITY;
    {
        S3G_TRY(auto w, data.words<20>("physics_animation", "default", "LandingOnDeckBLendVsTime"));
        const auto words = rs::from_bits(w);
        PointGraph<8> g;
        std::copy(words.begin() + 4, words.begin() + 12, g.x.begin());
        std::copy(words.begin() + 12, words.begin() + 20, g.y.begin());
        out.landing_on_board_blend = g;
    }
    out.animation_hips = IDENTITY;
    out.motion = sc::physics::skeleton_motion::SkeletonMotion{};
    out.board_at_y_delta = 0.0f;
    S3G_TRY(out.landing_settings, load_landing_settings(data));
    return out;
}

Status AnimatedSkeleton::process_pose(sc::animation::Slice<const AnimationPartTransform> globals,
                                      sc::physics::skeleton_landing::LandingInput landing_input, float dt,
                                      std::uint32_t& flags_2468, std::uint32_t& flags_2472,
                                      std::optional<LandingOnBoard> landing_on_board) {
    const AnimationPartTransform* trajectory = globals.first();
    if (trajectory == nullptr) return rs::make_err(std::string("Animation has no trajectory bone"));
    motion.process_trajectory(*trajectory, roots.animation_to_world, dt);
    auto mapped =
        sc::physics::skeleton_animation_record::map_animation_parts(globals.data(), globals.size(), bone_indices,
                                                                    physics_frames);
    if (mapped.is_err()) return rs::make_err(std::string(mapped.unwrap_err()));
    std::array<AnimationPartTransform, 24> parts = mapped.unwrap();
    unadjusted_board = parts[0];
    sc::physics::skeleton_motion::SkeletonMotion::publish_unadjusted_board(parts[0], flags_2472);
    const std::size_t target_parts[4] = {15, 19, 3, 7};
    for (std::size_t target = 0; target < 4; ++target) {
        const AnimationPartTransform* bone = globals.get(target_bones[target]);
        if (bone == nullptr) return rs::make_err(std::string("Missing animation IK target bone"));
        targets[target] = compose_affine(*bone, physics_frames[target_parts[target]]);
    }
    landing_input.animation_com_height = record.centre_of_mass[1] - parts[0][3][1];
    if (landing_on_board) {
        const auto& [deck, offset, flags2480, time] = *landing_on_board;
        if (auto adjustment = sc::physics::skeleton_landing_on_board::pose_adjustment(
                roots.world_to_animation, deck, parts[0], offset, board_frames.com_velocity[1], flags2480, time,
                landing_on_board_blend)) {
            board_offset.refresh_transform(*adjustment);
        }
    }
    landing.update(landing_input, landing_settings, board_offset);
    board_offset.update(parts[0], targets);
    record.update(parts, roots.animation_to_board, masses);
    // ProcessData82BD8E04..8E10 writes local board11728 AFTER COM.
    // World animation target15952 belongs to the later board-update phase.
    animation_board = parts[0];
    animation_hips = parts[23];
    board_at_y_delta = motion.publish_adjusted_board(parts[0], flags_2468);
    return ok();
}

void AnimatedSkeleton::update_roots(const sc::physics::board_runtime::BoardRuntime& board,
                                    const AnimationPartTransform& reckoning, float dt) {
    const auto deck = board.part_transforms()[sc::physics::board::index(BodyId::Deck)];
    const auto velocity = board.bodies()[sc::physics::board::index(BodyId::Deck)].rates.linear_velocity;
    AnimationPartTransform transform = IDENTITY;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t i = 0; i < 3; ++i) transform[axis][i] = deck.basis.columns[axis][i];
    }
    transform[3] = {deck.translation.x, deck.translation.y, deck.translation.z, 0.0f};
    roots.update(transform, {velocity.x, velocity.y, velocity.z, 0.0f}, dt, animation_board, reckoning);
}

AnimationPartTransform AnimatedSkeleton::prepare_ground(const sc::physics::board_runtime::BoardRuntime& board,
                                                        const AnimationPartTransform& reckoning, float dt,
                                                        std::uint32_t& flags_2468) {
    roots.initialize_heading = true;  //82BDF550, on every Ground update.
    update_roots(board, reckoning, dt);
    return board_frames.prepare_ground(roots, record.pose[0], roots.board, flags_2468);
}

void AnimatedSkeleton::finish_ground() { motion.next_trajectory = IDENTITY; }

rs::Result<sc::physics::skeleton_landing::LandingSettings, std::string> load_landing_settings(
    const sd::Collections& data) {
    auto value = [&](const char* name) { return data.float_("physics_animation", "default", name); };
    auto graph = [&](const char* name) -> rs::Result<PointGraph<4>, std::string> {
        // PointNegGraphData4 stores four bounds, followed by native X/Y arrays.
        S3G_TRY(auto w, data.words<12>("physics_animation", "default", name));
        const auto words = rs::from_bits(w);
        PointGraph<4> g;
        std::copy(words.begin() + 4, words.begin() + 8, g.x.begin());
        std::copy(words.begin() + 8, words.begin() + 12, g.y.begin());
        return g;
    };
    sc::physics::skeleton_landing::LandingSettings s;
    S3G_TRY(s.manual_blend, graph("LandingAdjustManualBlend"));
    S3G_TRY(s.grind_blend, graph("LandingAdjustGrindBlend"));
    S3G_TRY(s.coffin_height, graph("LandingAdjustCoffinBounceHeight"));
    S3G_TRY(s.minimum_height, value("LandingAdjustMinYPos"));
    S3G_TRY(s.maximum_velocity, value("LandingAdjustMaxVel"));
    S3G_TRY(s.manual_damping, value("LandingAdjustManualK2"));
    S3G_TRY(s.manual_spring, value("LandingAdjustManualK1"));
    S3G_TRY(s.ground_minimum_compression_time, value("LandingAdjustGroundMinCompressionTime"));
    S3G_TRY(s.ground_damping, value("LandingAdjustGroundK2"));
    S3G_TRY(s.ground_spring, value("LandingAdjustGroundK1"));
    S3G_TRY(s.grind_animation_target_time, value("LandingAdjustGrindStartUsingAnimTargetAfterTime"));
    S3G_TRY(s.grind_damping, value("LandingAdjustGrindK2"));
    S3G_TRY(s.grind_spring, value("LandingAdjustGrindK1"));
    S3G_TRY(s.grind_target_delta, value("LandingAdjustGrindBlendTowardsAnimTargetSpeed"));
    S3G_TRY(s.desired_com_height, value("LandingAdjustDesiredCom"));
    S3G_TRY(s.coffin_time, value("LandingAdjustCoffinBounceTime"));
    S3G_TRY(s.coffin_maximum_velocity, value("LandingAdjustCoffinBounceMaxVel"));
    S3G_TRY(s.coffin_blend_frames, value("LandingAdjustCoffinBounceFramesToBlendAway"));
    S3G_TRY(s.coffin_base_height, value("LandingAdjustCoffinBounceBaseHeight"));
    return s;
}

}  // namespace s3::game::physics::animated_skeleton
