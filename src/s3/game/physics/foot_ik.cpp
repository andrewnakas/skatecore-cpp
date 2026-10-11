// Port of skate-game/src/physics/foot_ik.rs.
#include "s3/game/physics/foot_ik.hpp"

#include <algorithm>
#include <vector>

#include "s3/core/animation/foot_ik/transforms.hpp"
#include "s3/game/physics/animated_skeleton.hpp"

namespace s3::game::physics::foot_ik {

namespace ik = sc::animation::foot_ik;
using sc::physics::skeleton_animation_record::compose_affine;

rs::Result<FootIk, std::string> FootIk::load(const sd::Collections& data, const sd::AnimationFrames& animation,
                                             const animated_skeleton::AnimatedSkeleton& skeleton) {
    //82BD3720 walks parents until it finds a represented physical part;
    //intermediate hierarchy bones do not become phantom physical joints.
    std::array<std::optional<std::size_t>, 24> parents{};
    for (std::size_t part = 0; part < 24; ++part) {
        const std::size_t bone = skeleton.bone_indices[part];
        if (bone >= animation.parents.size()) return rs::make_err(std::string("IK bone exceeds stock hierarchy"));
        std::int32_t ancestor = animation.parents[bone];
        std::vector<bool> visited(animation.parents.size(), false);
        while (ancestor >= 0) {
            const std::size_t index = static_cast<std::size_t>(ancestor);
            if (index >= visited.size() || visited[index])
                return rs::make_err(fmt::cat("Invalid stock IK hierarchy ancestor for part ", part));
            visited[index] = true;
            const auto& bi = skeleton.bone_indices;
            const auto found = std::find(bi.begin(), bi.end(), index);
            if (found != bi.end()) {
                parents[part] = static_cast<std::size_t>(found - bi.begin());
                break;
            }
            ancestor = animation.parents[index];
        }
    }
    S3G_TRY(auto geometry, ik::drive::Geometry::new_(parents, skeleton.physics_frames));
    S3G_CHECK(geometry.validate_limbs(ik::state::LIMBS));
    FootIk out;
    out.state = ik::state::FootIkState{};
    out.geometry = geometry;
    S3G_TRY(out.settings, load_settings(data));
    S3G_TRY(out.post_settings.minimum_board_up, data.float_("physics_animation", "default", "PostIKMinYAxisVal"));
    S3G_TRY(out.post_settings.wipeout_height, data.float_("animation", "default", "FeetRelativeHeightWipeout"));
    S3G_TRY(out.post_settings.riding_height, data.float_("animation", "default", "FeetRelativeHeightOnDeck"));
    out.bone_indices = skeleton.bone_indices;
    return out;
}

void FootIk::enable_feet(bool enabled) { state.enable_feet(enabled); }

std::array<std::array<float, 4>, 2> FootIk::physical_toe_positions(
    const sc::physics::skeleton_body::SkeletonPhysicalRecord& record) const {
    using sc::physics::skeleton_animation_record::transform_point;
    return {
        transform_point(record.pose[19], geometry.inverse_part_frames[20][3]),
        transform_point(record.pose[15], geometry.inverse_part_frames[16][3]),
    };
}

std::array<bool, 4> FootIk::post_physics(sc::physics::skeleton_body::SkeletonBody& body,
                                         ik::post_physics::Input input) {
    return ik::post_physics::update(state, body, geometry, settings, post_settings, input);
}

rs::Result<DriveOutput, std::string> FootIk::update(const animated_skeleton::AnimatedSkeleton& skeleton,
                                                    sc::animation::Slice<const Transform> globals,
                                                    PhysicalInput input) {
    std::array<Transform, 24> drives = skeleton.record.pose;
    //GeneralUpdate skips all IK stages for702 but still submits mapped
    //animation drives. Preserve histories across that skipped update.
    if (input.state_id != 702) {
        std::array<Transform, 24> originals = skeleton.record.pose;
        for (std::size_t part = 0; part < 24; ++part) {
            const Transform* bone = globals.get(bone_indices[part]);
            if (bone == nullptr)
                return rs::make_err(std::string("IK original joint is absent from the current pose"));
            originals[part] = *bone;
        }
        //UpdateIKData's503 branch uses the animated board in world space
        //for both board inputs; every other state consumes distinct caches.
        const Transform animated_world = compose_affine(skeleton.roots.animation_to_world, skeleton.record.pose[0]);
        const Transform special_inverse = ik::transforms::inverse_rigid(animated_world);
        const bool special = input.state_id == 503;
        const Transform& physical_board = special ? animated_world : *input.physical_board;
        const Transform& contact_board = special ? animated_world : skeleton.roots.board;
        const Transform& inverse_contact_board = special ? special_inverse : skeleton.roots.inverse_board;
        const ik::state::UpdateInput update_input{
            input.flags_2468,
            input.flags_2472,
            input.flags_2480,
            skeleton.roots.animation_to_world,
            skeleton.roots.world_to_animation,
            physical_board,
            contact_board,
            inverse_contact_board,
            skeleton.record.pose,
            originals,
            skeleton.targets,
            input.hips_world_position,
            input.contact_bone,
            {bone_indices[15], bone_indices[19]},
            input.current_contacts,
        };
        (void)state.update(update_input, geometry, settings, drives);
    }
    return DriveOutput{drives};
}

rs::Result<ik::settings::Settings, std::string> load_settings(const sd::Collections& data) {
    auto vector = [&](const char* name) -> rs::Result<std::array<float, 4>, std::string> {
        S3G_TRY(auto words, data.words<4>("physics_skeletonik", "default", name));
        return rs::from_bits(words);
    };
    auto scalar = [&](const char* name) { return data.float_("physics_skeletonik", "default", name); };
    S3G_TRY(const float width, data.float_("physicsdeck", "default", "DeckWidth"));
    const float half_width = width * 0.5f;
    S3G_TRY(const float mid, data.float_("physicsdeck", "default", "DeckMidLength"));
    const float half_length = mid * 0.5f;
    ik::settings::Settings s;
    S3G_TRY(s.angle_limits.minimum_degrees, data.float_("physics_animation", "default", "IKMinAngle"));
    S3G_TRY(s.angle_limits.maximum_degrees, data.float_("physics_animation", "default", "IKMaxAngle"));
    S3G_TRY(s.blend.hand_inner_padding, vector("IKOnPadding"));
    S3G_TRY(s.blend.hand_outer_padding, vector("IKOffPadding"));
    S3G_TRY(s.blend.external_blend_step, scalar("FeetIKMaxBlendDeltaOut"));
    S3G_TRY(s.blend.board_blend_step, scalar("FeetIKMaxBlendDelta"));
    S3G_TRY(s.post_ik_padding, vector("PostIKPadding"));
    S3G_TRY(s.contact_bounds, vector("IKBoneOnDeckBBox"));
    S3G_TRY(s.foot_on_deck_padding, vector("FootOnDeckPadding"));
    S3G_TRY(s.wipeout_feet_offset, scalar("WipeoutFeetOffset"));
    s.deck_half_width = half_width;
    s.deck_half_length = half_length;
    // Source uses FrontEndSize here, while the deck collider's fan loop
    // uses BackEndSize. They are distinct authored parameters.
    S3G_TRY(const float front, data.float_("physicsdeck", "default", "DeckFrontEndSize"));
    s.deck_total_half_length = front + half_length;
    S3G_TRY(s.deck_front_angle_degrees, data.float_("physicsdeck", "default", "DeckFrontEndAngle"));
    return s;
}

}  // namespace s3::game::physics::foot_ik
