// Port of skate-game/src/skater_animation.rs.
#include "s3/game/skater_animation.hpp"

#include <array>

#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/animation/posture.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/outputs.hpp"
#include "s3/game/graph_runtime/disk.hpp"

namespace s3::game::skater_animation {

namespace {

using s3::animation::playback_tree::Evaluation;
using s3::animation::playback_tree::PoseCommand;
using s3::animation::skeleton_input::name::encode;

/// Rust `{:?}` of `BufferError`.
std::string debug_buffer_error(const s3::animation::commands::buffers::BufferError& error) {
    using E = s3::animation::commands::buffers::BufferError;
    if (std::holds_alternative<E::UnknownBuffer>(error.value)) return "UnknownBuffer";
    if (std::holds_alternative<E::RangeOutsideAllocation>(error.value)) return "RangeOutsideAllocation";
    if (const auto* bone = std::get_if<E::UninitializedBone>(&error.value)) {
        return "UninitializedBone { bone: " + std::to_string(bone->bone) + " }";
    }
    return "UnsupportedPartialOverlap";
}

rs::Result<std::shared_ptr<const AnimationSource>, std::string> build_source(s3::data::AnimationBanks banks,
                                                                             PoseEvaluator evaluator) {
    auto source = std::make_shared<AnimationSource>();
    source->banks = std::move(banks);
    source->evaluator = std::make_shared<const PoseEvaluator>(std::move(evaluator));
    return std::shared_ptr<const AnimationSource>(std::move(source));
}

}  // namespace

rs::Result<std::shared_ptr<const AnimationSource>, std::string> AnimationSource::load(const std::string& root) {
    const s3::data::Vfs vfs = graph_runtime::disk::read_banks(root);
    auto banks = s3::data::AnimationBanks::load(vfs, root);
    if (!banks.ok) return rs::make_err(banks.error);
    auto evaluator = PoseEvaluator::from_banks(banks.value);
    if (evaluator.is_err()) return rs::make_err(evaluator.unwrap_err());
    auto clips = evaluator.unwrap().load_authored_clips(root);
    if (clips.is_err()) return rs::make_err(clips.unwrap_err());
    return build_source(std::move(banks.value), std::move(evaluator.unwrap()));
}

rs::Result<std::shared_ptr<const AnimationSource>, std::string> AnimationSource::load(const s3::data::Vfs& vfs,
                                                                                    const std::string& root) {
    auto banks = s3::data::AnimationBanks::load(vfs, root);
    if (!banks.ok) return rs::make_err(banks.error);
    auto evaluator = PoseEvaluator::from_banks(banks.value);
    if (evaluator.is_err()) return rs::make_err(evaluator.unwrap_err());
    auto clips = evaluator.unwrap().load_authored_clips(vfs, root);
    if (clips.is_err()) return rs::make_err(clips.unwrap_err());
    return build_source(std::move(banks.value), std::move(evaluator.unwrap()));
}

std::uint32_t SkaterAnimation::checkpoint_stance() const {
    const auto& p = state.publication;
    return static_cast<std::uint32_t>(((p.natural_stance == 0 && p.relative_stance == 0) ||
                                       (p.natural_stance == 1 && p.relative_stance == 1)) ^
                                      state.fakie());
}

void SkaterAnimation::request_checkpoint_stance(std::uint32_t foot) {
    const std::int32_t natural = state.publication.natural_stance;
    motion.animation.requested_stance = static_cast<std::uint32_t>(foot == 0 ? natural != 1 : natural == 1);
}

void SkaterAnimation::set_customisation(std::uint32_t natural, std::uint32_t style) {
    if (natural <= 1 && state.publication.natural_stance != static_cast<std::int32_t>(natural)) {
        state.publication.natural_stance = static_cast<std::int32_t>(natural);
        state.flags ^= 0xC0000000u;
    }
    // GetCACSettings82590D20..D84: 0=null,1=Loose,2=Gonzo,3=Aggressive.
    const char* name = "";
    switch (style) {
        case 1: name = "Loose"; break;
        case 2: name = "Gonzo"; break;
        case 3: name = "Aggressive"; break;
        default: break;
    }
    motion.playback_context.pro_skater = encode(name);
}

rs::Result<std::vector<NativeMatrix>, std::string> SkaterAnimation::evaluate_initial_pose() {
    const std::vector<PoseCommand> commands = {PoseCommand::Pose{"RIG_TPOSE"}};
    auto evaluated = evaluator->evaluate(commands);
    if (evaluated.is_err()) return rs::make_err(evaluated.unwrap_err());
    pose = std::move(evaluated.unwrap());
    return evaluator->hierarchy(pose);
}

rs::Result<SkaterAnimation, std::string> SkaterAnimation::load(const std::string& root,
                                                               const s3::data::Collections& data,
                                                               const StockGraphs& graphs,
                                                               std::string_view pro_skater) {
    auto source = AnimationSource::load(root);
    if (source.is_err()) return rs::make_err(source.unwrap_err());
    return from_source(data, graphs, pro_skater, source.unwrap());
}

rs::Result<SkaterAnimation, std::string> SkaterAnimation::load(const s3::data::Vfs& vfs, const std::string& root,
                                                               const s3::data::Collections& data,
                                                               const StockGraphs& graphs,
                                                               std::string_view pro_skater) {
    auto source = AnimationSource::load(vfs, root);
    if (source.is_err()) return rs::make_err(source.unwrap_err());
    return from_source(data, graphs, pro_skater, source.unwrap());
}

rs::Result<SkaterAnimation, std::string> SkaterAnimation::from_source(const s3::data::Collections& data,
                                                                      const StockGraphs& graphs,
                                                                      std::string_view pro_skater,
                                                                      std::shared_ptr<const AnimationSource> source) {
    std::shared_ptr<const PoseEvaluator> evaluator = source->evaluator;
    state::AnimationState state = state::AnimationState::new_(true);
    const std::size_t count = evaluator->frames.bone_names.size();
    auto metadata = source->banks.metadata();
    if (!metadata.ok) return rs::make_err(metadata.error);
    s3::animation::playback::PlaybackContext context{};
    context.is_switch = state.switch_();
    context.is_mirrored = state.mirrored();
    context.board_available = std::nullopt;
    context.pro_skater = encode(pro_skater);
    context.transition_override = std::nullopt;
    auto created = MotionHost::from_graph(graphs.motion, data, std::move(metadata.value), std::move(context));
    if (created.is_err()) return rs::make_err("Stock MotionGraph initialization: " + created.unwrap_err());
    MotionHost motion = std::move(created.unwrap());
    auto hierarchy = motion.animation.set_hierarchy(evaluator->frames.bone_names, evaluator->frames.mirror_indices);
    if (hierarchy.is_err()) return rs::make_err(std::string(hierarchy.unwrap_err()));
    //82858810 registers these three static poses in this exact order.
    using s3::animation::posture::PosturePose;
    for (const PosturePose posture : {PosturePose::Stiff, PosturePose::Slouch, PosturePose::Buff}) {
        const char* name = s3::animation::posture::posture_pose_name(posture);
        auto pose = evaluator->frames.named_pose(name);
        if (!pose.ok) return rs::make_err(pose.error);
        if (pose.value->samples.size() != count) {
            return rs::make_err("Stock posture " + std::string(name) + " has " +
                                std::to_string(pose.value->samples.size()) + " bones; skater hierarchy has " +
                                std::to_string(count));
        }
    }
    motion.animation.posture_bank_valid = true;
    motion.animation.skater_animation_flags = state.flags;
    auto action = ActionHost::from_graph(graphs.action, data);
    if (action.is_err()) return rs::make_err("Stock ActionGraph initialization: " + action.unwrap_err());
    SkaterAnimation result(std::move(action.unwrap()), std::move(motion));
    result.evaluator = std::move(evaluator);
    result.source = std::move(source);
    result.pose.clear();
    result.attributes = PacketAttributes{};
    result.state = state;
    result.action_controller = Controller::new_(graphs.action.runtime.program.topology.states.size());
    result.motion_controller = Controller::new_(graphs.motion.runtime.program.topology.states.size());
    //Represented pose fields after AnimOutPhysIn Reset82590028.
    using s3::animation::output::packet_reset::RESET_POSE;
    result.packet.bone_count = static_cast<std::uint32_t>(count);
    result.packet.hierarchy.assign(count, RESET_POSE);
    result.packet.local.assign(count, RESET_POSE);
    result.packet.timestep = rs::from_bits(0x3c888889);
    result.packet.foot_surface_ids = {0, 0};
    result.packet.flags = 0;
    result.packet.board_flipped = false;
    result.packet.mirrored = false;
    result.packet.riding_switch = false;
    result.packet.riding_fakie = false;
    result.packet.weight_forwards = false;
    result.packet.regular_stance = false;
    result.packet.air_dismount_revert_frames = 0;
    result.ticks = 0;
    return std::move(result);
}

rs::Result<rs::Unit, std::string> SkaterAnimation::advance(const StockGraphs& graphs, float dt,
                                                           const IntentMap& action_intents,
                                                           AnimationPhysical physical,
                                                           AdditionalResetFields& reset_fields) {
    using namespace graph_host::outputs;
    const std::uint64_t tick = ticks;
    {
        auto published = publish_physical(std::move(physical));
        if (published.is_err()) return published;
    }
    ActionGraphInput input;
    input.tick = tick;
    input.controls = ActionInput::from_values(action_intents);
    input.prior_motion = PriorMotionState::from_values(motion.animation.motion_intents);
    //HasAnimAttribute82BA3A78 reads the preceding cached animation
    //attributes. Specific flag27 is observed before this frame's MG update.
    const auto tree = motion.animation.tree_attributes();
    input.animation_attributes.assign(tree.begin(), tree.end());
    action.prepare_input(std::move(input));
    action.is_tricking = motion.flags.doing_trick;
    action_controller.update(graphs.action.runtime.program, dt, action);
    if (!action.errors.is_empty()) return rs::make_err(action.errors.join("\n"));
    ActionGraphOutput action_output = action.output();
    motion.animation.begin_graph_update();
    MotionGraphInput motion_input;
    motion_input.tick = action_output.tick;
    motion_input.action = std::move(action_output);
    motion.accept_action_graph(std::move(motion_input));
    motion.animation_phase = state.phase;
    motion_controller.update(graphs.motion.runtime.program, dt, motion);
    state.phase = motion.animation_phase;
    if (!motion.errors.is_empty()) return rs::make_err(motion.errors.join("\n"));
    if (!motion.animation.skater_animation_flags) {
        return rs::make_err(std::string("MotionGraph lost the SkaterAnim flag owner"));
    }
    state.flags = *motion.animation.skater_animation_flags;
    state.publication.relative_stance = static_cast<std::int32_t>(motion.animation.relative_stance);
    if (std::exchange(motion.animation.reset_action_intents, false)) action.action_intents.clear();

    {
        auto applied = motion.animation.apply_parameters();
        if (applied.is_err()) return rs::make_err(std::string(applied.unwrap_err()));
    }
    motion.animation.advance(dt, state.phase);
    //Collect before pose evaluation consumes clip history;82B98980 then
    //uses those exact records for board/mirror/switch event publication.
    {
        auto refreshed = motion.animation.refresh_tree_attributes();
        if (refreshed.is_err()) return rs::make_err(std::string(refreshed.unwrap_err()));
    }
    state.apply_stance_events(motion.animation.tree_attributes());
    Evaluation evaluation{};
    evaluation.cull_threshold = state.cull_threshold();
    evaluation.update_history = true;
    auto commands = motion.animation.evaluate_pose(evaluation);
    if (commands.is_err()) return rs::make_err(std::string(commands.unwrap_err()));
    auto evaluated = evaluator->evaluate(commands.unwrap());
    if (evaluated.is_err()) return rs::make_err(evaluated.unwrap_err());
    pose = std::move(evaluated.unwrap());
    auto hierarchy = evaluator->hierarchy(pose);
    if (hierarchy.is_err()) return rs::make_err(hierarchy.unwrap_err());
    std::vector<NativeMatrix> local;
    local.reserve(pose.size());
    for (const Sqt& sqt : pose) local.push_back(s3::animation::output::sqt_to_matrix(sqt));
    attributes.replace_from(motion.animation.motion_attributes, motion.animation.tree_attributes());
    state.prepare_publication();
    // Actor SetUpPhysics82593640 resets the output after animation has
    // evaluated, before publishing this pose and the actor-owned fields.
    {
        auto reset = s3::animation::output::packet_reset::reset(packet, reset_fields);
        if (reset.is_err()) return rs::make_err("Animation packet reset: " + debug_buffer_error(reset.unwrap_err()));
    }
    {
        static constexpr std::uint8_t SIGNUP[] = {'s', 'i', 'g', 'n', 'u', 'p'};
        auto published = s3::animation::output::physics_packet::publish_evaluated(
            state.publication, hierarchy.unwrap(), local, packet,
            //Actor825936AC..D4 passes this fixed physical step even when
            //the graph/clip clock differs or camera slow motion is active.
            static_cast<double>(rs::from_bits(0x3c888889)), SIGNUP);
        if (published.is_err()) {
            return rs::make_err("Skater pose publication: " + debug_buffer_error(published.unwrap_err()));
        }
    }
    state.finish_publication();
    motion.animation.skater_animation_flags = state.flags;
    ticks += 1;
    return rs::Unit{};
}

rs::Result<rs::Unit, std::string> SkaterAnimation::publish_physical(AnimationPhysical p) {
    motion.animation.natural_stance = static_cast<std::uint32_t>(state.publication.natural_stance);
    motion.animation.relative_stance = static_cast<std::uint32_t>(state.publication.relative_stance);
    if (!p.conditions.speeds) return rs::make_err(std::string("Animation requires completed board speed output"));
    const auto speed = *p.conditions.speeds;
    const std::pair<bool, bool> stance_ = {state.fakie(), state.mirrored()};
    action.stance = stance_;
    action.condition_inputs = p.conditions;
    motion.condition_inputs = std::move(p.conditions);
    motion.playback_context.is_mirrored = state.mirrored();
    motion.playback_context.is_switch = state.switch_();
    motion.playback_context.board_available = p.board_present;
    MotionPhysical physical{};
    physical.turning = p.feedback.turning;
    physical.stance = stance_;
    physical.forward_speed = speed.forward_speed;
    physical.time_since_teleport = p.time_since_teleport;
    physical.is_switch = state.switch_();
    physical.foot_frame = p.foot_frame;
    motion.physical = physical;
    motion.physical_stance = p.physical_stance;
    motion.crouching_physical = p.feedback.crouching;
    motion.body_tilt_physical = p.body_tilt;
    motion.fakie_physical = p.fakie;
    motion.pumping_acceleration = p.feedback.pumping_acceleration;

    //8259303C..78: on-board category keeps this true even when the
    //off-board owner's board-present byte is false. 82B971A8 copies it
    //straight into flag17. It is not a no-board bit.
    const bool board_attached_or_onboard = p.board_present || !p.physical_28_byte75;
    state.flags = (state.flags & ~(1u << 17)) | (static_cast<std::uint32_t>(board_attached_or_onboard) << 17);
    motion.animation.skater_animation_flags = state.flags;
    return rs::Unit{};
}

}  // namespace s3::game::skater_animation
