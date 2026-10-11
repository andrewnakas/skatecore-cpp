// Port of skate-game/src/skater_animation.rs.
//! The skater's stock graph -> tree -> pose -> physics-packet owner.
//! Physical observations come from the preceding completed simulation output.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "s3/core/animation/body_tilt.hpp"
#include "s3/core/animation/output.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/packet_reset.hpp"
#include "s3/core/animation/output/physics_packet.hpp"
#include "s3/core/animation/physical_feedback.hpp"
#include "s3/core/animation/riding_fakie.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/core/graph/controller.hpp"
#include "s3/core/graph/intents.hpp"
#include "s3/core/riding/push_behaviors.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/animation_pose.hpp"
#include "s3/game/graph_host/action.hpp"
#include "s3/game/graph_host/motion.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/skater_animation/state.hpp"

namespace s3::game::skater_animation {

using animation_pose::PoseEvaluator;
using graph_host::action::ActionHost;
using graph_host::motion::MotionHost;
using graph_host::motion::MotionPhysical;
using graph_runtime::StockGraphs;
using s3::animation::output::NativeMatrix;
using s3::animation::output::Sqt;
using s3::animation::output::attributes::PacketAttributes;
using s3::animation::output::packet_reset::AdditionalResetFields;
using s3::animation::output::physics_packet::PhysicsPosePacket;
using s3::animation::physical_feedback::PhysicalFeedback;
using s3::graph::conditions::ConditionInputs;
using s3::graph::controller::Controller;
using s3::graph::intents::IntentMap;
using s3::riding::push_behaviors::PushFootFrame;

/// Immutable bank bytes and decoded poses survive world changes. (`pub(crate)` in Rust.)
/// Rust `Arc<AnimationSource>` / `Arc<PoseEvaluator>` are `std::shared_ptr<const ...>`.
struct AnimationSource {
    // Private in Rust.
    s3::data::AnimationBanks banks;
    std::shared_ptr<const PoseEvaluator> evaluator;

    /// Private in Rust. `root` is a filesystem directory (Rust `&Path`): reads both stock
    /// banks and the optional authored clip file from disk.
    static rs::Result<std::shared_ptr<const AnimationSource>, std::string> load(const std::string& root);
    /// Same, reading the bank and authored clip files from an existing Vfs.
    static rs::Result<std::shared_ptr<const AnimationSource>, std::string> load(const s3::data::Vfs& vfs,
                                                                                const std::string& root);
};

/// Completed native publications, never inferred from a render transform. (`pub(crate)` in Rust.)
struct AnimationPhysical {
    ConditionInputs conditions;
    PhysicalFeedback feedback;
    s3::animation::body_tilt::Physical body_tilt;
    s3::animation::riding_fakie::Physical fakie;
    ///PhysOutAnimation158/157, consumed by IsRidingGoofy.
    std::pair<bool, bool> physical_stance{false, false};
    PushFootFrame foot_frame;
    ///PhysOut72 byte311 (true means a board is present).
    ///both PlayAnimation82BB54D4 and Phase1 consume the byte without inversion.
    bool board_present = false;
    bool physical_28_byte75 = false;
    float time_since_teleport = 0.0f;
};

/// `pub(crate)` in Rust.
class SkaterAnimation {
public:
    ActionHost action;
    MotionHost motion;
    std::shared_ptr<const PoseEvaluator> evaluator;
    std::shared_ptr<const AnimationSource> source;
    std::vector<Sqt> pose;
    PhysicsPosePacket packet;
    PacketAttributes attributes;
    Controller action_controller;
    Controller motion_controller;
    // Private in Rust (kept accessible; no name clashes).
    state::AnimationState state;
    std::uint64_t ticks = 0;

    ///82592B68: physical leading foot, including the fakie inversion.
    std::uint32_t checkpoint_stance() const;
    ///82592C08/82B97350: save15200; ResetToGivenStance consumes it later.
    void request_checkpoint_stance(std::uint32_t foot);
    /// Manual markers use the same native leading-foot query as bail checkpoints.
    bool foot_forward() const { return checkpoint_stance() != 0; }
    /// Queue the native stance request; the teleport graph applies it after reset.
    void restore_foot_forward(bool forward) { request_checkpoint_stance(static_cast<std::uint32_t>(forward)); }
    /// Native Initialize82B97E38 selects both orientation/mirror bits for
    /// regular stance. A profile edit changes the natural basis while retaining
    /// the current relative stance and trick state. (`pub(crate)` in Rust.)
    void set_customisation(std::uint32_t natural, std::uint32_t style);
    std::pair<bool, bool> stance() const { return {state.fakie(), state.mirrored()}; }
    /// Actor8259147C evaluates the stored initialization pose before creating
    /// the physical skeleton. Initialize82B97E38 selects rig_Tpose;
    /// 82D181D0 stores it at13484, and82531050 emits that pose command.
    /// This is separate from both the graph's first update and AnimOut reset.
    rs::Result<std::vector<NativeMatrix>, std::string> evaluate_initial_pose();

    /// `root` is a filesystem directory (Rust `&Path`).
    static rs::Result<SkaterAnimation, std::string> load(const std::string& root, const s3::data::Collections& data,
                                                         const StockGraphs& graphs, std::string_view pro_skater);
    /// Same, reading the animation source files from an existing Vfs.
    static rs::Result<SkaterAnimation, std::string> load(const s3::data::Vfs& vfs, const std::string& root,
                                                         const s3::data::Collections& data,
                                                         const StockGraphs& graphs, std::string_view pro_skater);

    static rs::Result<SkaterAnimation, std::string> from_source(const s3::data::Collections& data,
                                                                const StockGraphs& graphs,
                                                                std::string_view pro_skater,
                                                                std::shared_ptr<const AnimationSource> source);

    ///Actor Phase1/2/3 plus SetUpPhysics:82592FD8,82593128,82593230,82593640.
    ///GenerateActionGraphIntents has completed before this call. Keep one MG
    ///intent map across both controllers; native preupdate clears only attrs.
    rs::Result<rs::Unit, std::string> advance(const StockGraphs& graphs, float dt, const IntentMap& action_intents,
                                              AnimationPhysical physical, AdditionalResetFields& reset_fields);

    /// Private in Rust.
    rs::Result<rs::Unit, std::string> publish_physical(AnimationPhysical p);

private:
    SkaterAnimation(ActionHost action_, MotionHost motion_)
        : action(std::move(action_)), motion(std::move(motion_)) {}
};

}  // namespace s3::game::skater_animation
