// Port of skate-game/src/physics/skeleton_output.rs.
#include "s3/game/physics/skeleton_output.hpp"

#include <algorithm>
#include <optional>

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/contact_feedback.hpp"
#include "s3/core/physics/truck_frames.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/game/physics/animated_skeleton.hpp"

namespace s3::game::physics::skeleton_output {

using sc::physics::board::BodyId;
using sc::point_graph::PointGraph;
namespace board = sc::physics::skeleton_output::board;
namespace wobble = sc::physics::skeleton_output::wobble;

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

rs::Result<SkeletonOutput, std::string> SkeletonOutput::load(const sd::Collections& data,
                                                             const sd::AnimationFrames& animation,
                                                             const animated_skeleton::AnimatedSkeleton& skeleton) {
    std::array<std::optional<std::size_t>, 24> parents{};
    for (std::size_t part = 0; part < 24; ++part) {
        const std::size_t bone = skeleton.bone_indices[part];
        if (bone >= animation.parents.size()) return rs::make_err(std::string("Missing output bone ancestor"));
        std::int32_t ancestor = animation.parents[bone];
        std::size_t visited = 0;
        while (ancestor >= 0) {
            const std::size_t index = static_cast<std::size_t>(ancestor);
            if (index >= animation.parents.size() || visited >= animation.parents.size())
                return rs::make_err(std::string("Invalid physical output hierarchy"));
            const auto& bi = skeleton.bone_indices;
            const auto found = std::find(bi.begin(), bi.end(), index);
            if (found != bi.end()) {
                parents[part] = static_cast<std::size_t>(found - bi.begin());
                break;
            }
            ancestor = animation.parents[index];
            visited += 1;
        }
    }
    auto bone = [&](const std::string& name) -> rs::Result<std::size_t, std::string> {
        for (std::size_t i = 0; i < animation.bone_names.size(); ++i)
            if (eq_ignore_ascii_case(animation.bone_names[i], name)) return i;
        return rs::make_err(fmt::cat("Missing stock output bone ", name));
    };
    auto scalar = [&](const char* key) { return data.float_("physics_skeleton", "default", key); };
    //82BD7DD0..7E88 names and stores these six ids; no authored index literals.
    board::BoneIndices board_bones;
    S3G_TRY(board_bones.front_truck, bone("Truck_Front"));
    S3G_TRY(board_bones.back_truck, bone("Truck_Back"));
    S3G_TRY(board_bones.front_left_wheel, bone("Left_WheelFront"));
    S3G_TRY(board_bones.front_right_wheel, bone("Right_WheelFront"));
    S3G_TRY(board_bones.back_left_wheel, bone("Left_WheelBack"));
    S3G_TRY(board_bones.back_right_wheel, bone("Right_WheelBack"));
    auto graph = [&](const char* key) -> rs::Result<PointGraph<8>, std::string> {
        S3G_TRY(auto w, data.words<20>("physics_deck_wobble", "default", key));
        const auto words = rs::from_bits(w);
        PointGraph<8> g;
        std::copy(words.begin() + 4, words.begin() + 12, g.x.begin());
        std::copy(words.begin() + 12, words.begin() + 20, g.y.begin());
        return g;
    };
    SkeletonOutput out;
    out.pose.bone_indices = skeleton.bone_indices;
    S3G_TRY(out.pose.geometry, sc::animation::foot_ik::drive::Geometry::new_(parents, skeleton.physics_frames));
    out.pose.board_bones = board_bones;
    S3G_TRY(out.pose.board_settings.truck_tilt_scalar, scalar("TruckTiltScalar"));
    S3G_TRY(out.pose.board_settings.truck_tilt_max_angle, scalar("TruckTiltMaxAngle"));
    S3G_TRY(out.pose.board_settings.truck_tilt_wobble_scalar, scalar("TruckTiltWobbleScalar"));
    S3G_TRY(out.pose.board_settings.truck_displacement_max, scalar("TruckDisplacementMax"));
    out.wobble = wobble::Wobble{};
    out.deck_wobble = wobble::Output{};
    out.correction = sc::physics::skeleton_output::correction::CorrectionState{};
    //8289F128 global220 = AF781D7B614DAB50 physicstrucks/default;
    //82C08D04..18 copies layout16 to SkateboardBody8380.
    S3G_TRY(out.compression_rest_height, data.float_("physicstrucks", "default", "TruckYPos"));
    S3G_TRY(out.wobble_settings.takeoff_tilt, graph("TiltVsTimeTakeOff"));
    S3G_TRY(out.wobble_settings.landing_tilt, graph("TiltVsTimeLanding"));
    S3G_TRY(out.wobble_settings.takeoff_squish, graph("SquishVsTimeTakeOff"));
    S3G_TRY(out.wobble_settings.landing_squish, graph("SquishVsTimeLanding"));
    S3G_TRY(out.wobble_settings.maximum_time, data.float_("physics_deck_wobble", "default", "MaxTime"));
    return out;
}

void SkeletonOutput::trigger_wobble(bool landing, bool reverse) { wobble.trigger(landing, reverse); }

std::array<float, 2> SkeletonOutput::average_compressions(const sc::physics::board_runtime::BoardRuntime& board) const {
    const Transform deck = to_matrix(board.body_transform(BodyId::Deck));
    std::array<std::array<float, 4>, 4> wheels{};
    for (std::size_t i = 0; i < 4; ++i) wheels[i] = to_matrix(board.body_transform(sc::physics::board::BodyId_ORDER[i]))[3];
    return sc::physics::contact_feedback::compression::average_wheel_compressions(deck, wheels,
                                                                                  compression_rest_height);
}

Transform SkeletonOutput::advance_wobble(sc::physics::skeleton_body::SkeletonBody& physical, const Transform& deck) {
    deck_wobble = wobble.update(wobble_settings);
    if (deck_wobble.sampled) {
        wobble::apply(deck_wobble, physical.record.pose[0]);
        return physical.record.pose[0];
    }
    return deck;
}

Status SkeletonOutput::publish(const animated_skeleton::AnimatedSkeleton& skeleton,
                               const sc::physics::skeleton_body::SkeletonBody& physical,
                               const sc::physics::board_runtime::BoardRuntime& board_runtime,
                               std::array<sc::physics::drive_frames::RetailAffineTransform, 2> base_trucks,
                               std::array<float, 2> current_truck_targets, std::array<float, 2> average_compression,
                               sc::animation::Slice<Transform> globals, sc::animation::Slice<Transform> locals) const {
    std::array<Transform, 6> bodies{};
    for (std::size_t i = 0; i < 6; ++i)
        bodies[i] = to_matrix(board_runtime.body_transform(sc::physics::board::BodyId_ORDER[i]));
    const auto truck_affine = sc::physics::truck_frames::steering_truck_transforms(base_trucks, current_truck_targets);
    const std::array<Transform, 2> trucks = {to_matrix(truck_affine[0]), to_matrix(truck_affine[1])};
    std::array<Transform, 24> physical_parts{};
    for (std::size_t i = 0; i < 24; ++i) physical_parts[i] = physical.record.pose[i];
    const sc::physics::skeleton_output::Input input{
        physical_parts,
        skeleton.roots.world_to_animation,
        board::Input{bodies, physical_parts[0], trucks, deck_wobble.tilt, deck_wobble.squish, average_compression},
    };
    auto result = pose.publish(input, sc::physics::Slice<Transform>(globals.data(), globals.size()),
                               sc::physics::Slice<Transform>(locals.data(), locals.size()));
    if (result.is_err()) return rs::make_err(std::string(result.unwrap_err()));
    return ok();
}

Transform to_matrix(sc::physics::drive_frames::RetailAffineTransform value) {
    Transform result{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t i = 0; i < 3; ++i) result[axis][i] = value.basis.columns[axis][i];
    }
    result[3] = {value.translation.x, value.translation.y, value.translation.z, 0.0f};
    return result;
}

}  // namespace s3::game::physics::skeleton_output
