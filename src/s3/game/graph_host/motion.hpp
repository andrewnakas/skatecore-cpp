// Port of skate-game/src/graph_host/motion.rs.
// Persistent host called by the production stock MotionGraph controller.
//
// Rust `#[path]` submodules declared by motion.rs (each its own header/source under motion/):
//   air_execution    (motion_air_execute.rs)     -> MotionHost::execute_air
//   body_tweak       (motion_offboard/body_tweak.rs)
//   execution        (motion_execute.rs)         -> MotionHost::execute
//   fakie_hold       (motion_fakie_hold.rs)
//   filter_execution (motion_filter_execute.rs)  -> MotionHost::execute_intent_filter
//   leaf_execution   (motion_execute_leaf.rs)    -> motion::leaf_execution::execute
//   match_air_time   (motion_offboard/match_air_time.rs)
//   score_execution  (motion_score_execute.rs)   -> MotionHost::execute_score_or_bump
//   slide_execution  (motion_sliding_execute.rs) -> MotionHost::execute_slide / slide_direction
//   toggle_execution (motion_toggle_execute.rs)  -> MotionHost::execute_toggle_board
//   wipeout_execution(motion_wipeout_execute.rs) -> MotionHost::execute_wipeout / execute_twist_lean
//   instances        (motion_instances.rs)       -> motion::instances::Instance
// Rust `impl MotionHost` blocks spread over those files are member functions of MotionHost
// defined in the corresponding motion/<name>.cpp.
//
// Graph advance entry point (as in Rust): `s3::graph::controller::Controller::update(program, dt,
// host)` with this host and `graph.runtime.program`.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/animation/air_leg_extension.hpp"
#include "s3/core/animation/body_tilt.hpp"
#include "s3/core/animation/crouching.hpp"
#include "s3/core/animation/kickturn.hpp"
#include "s3/core/animation/manual.hpp"
#include "s3/core/animation/playback.hpp"
#include "s3/core/animation/pumping_channel.hpp"
#include "s3/core/animation/riding_fakie.hpp"
#include "s3/core/graph/activation.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/core/graph/controller.hpp"
#include "s3/core/graph/intents.hpp"
#include "s3/core/input/set_turning.hpp"
#include "s3/core/riding/push_behaviors.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/animation_metadata.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/graph_host/motion/instances.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_bump.hpp"
#include "s3/game/graph_host/motion_character_gesture.hpp"
#include "s3/game/graph_host/motion_finger_flip.hpp"
#include "s3/game/graph_host/motion_gameplay_conditions.hpp"
#include "s3/game/graph_host/motion_grind.hpp"
#include "s3/game/graph_host/motion_hand_services.hpp"
#include "s3/game/graph_host/motion_hippy_jump.hpp"
#include "s3/game/graph_host/motion_intent_filter.hpp"
#include "s3/game/graph_host/motion_landing.hpp"
#include "s3/game/graph_host/motion_native.hpp"
#include "s3/game/graph_host/motion_nodes.hpp"
#include "s3/game/graph_host/motion_riding.hpp"
#include "s3/game/graph_host/motion_riding_conditions.hpp"
#include "s3/game/graph_host/motion_runout.hpp"
#include "s3/game/graph_host/motion_shove.hpp"
#include "s3/game/graph_host/motion_sliding.hpp"
#include "s3/game/graph_host/motion_spin.hpp"
#include "s3/game/graph_host/motion_stock_gameplay.hpp"
#include "s3/game/graph_host/motion_toggle_board.hpp"
#include "s3/game/graph_host/motion_tricks.hpp"
#include "s3/game/graph_host/motion_twist_lean.hpp"
#include "s3/game/graph_host/motion_wipeout.hpp"
#include "s3/game/graph_host/outputs.hpp"
#include "s3/game/graph_host/pushing.hpp"
#include "s3/game/graph_host/pushing_settings.hpp"
#include "s3/game/graph_runtime.hpp"

namespace s3::game::graph_host::motion {

// pub use super::motion_animation::MotionAnimation;
using motion_animation::MotionAnimation;
using instances::Instance;
using motion_nodes::MotionFactory;
using motion_nodes::MotionOperation;
using outputs::ActionControls;
using outputs::GraphCapabilityReport;
using outputs::GraphDiagnostics;
using outputs::GraphEffects;
using outputs::MotionGraphInput;
using outputs::TurningOutput;
using graph_runtime::LoadedGraph;
using graph_runtime::OperationRemap;
using pushing::PushContext;
using pushing::PushInstance;
using pushing_settings::PushingSettings;
using s3::graph::conditions::ConditionInputs;
using s3::graph::controller::BehaviorId;
using s3::graph::controller::Frame;
using s3::graph::controller::HookId;
using s3::graph::intents::IntentMap;
using s3::animation::playback::PlayAnimationInstance;
using s3::animation::playback::PlaybackContext;
using s3::riding::push_behaviors::PushFootFrame;
using s3::riding::push_behaviors::PushState;

struct MotionPhysical {
    s3::input::set_turning::Physical turning;
    std::pair<bool, bool> stance{false, false};
    float forward_speed = 0.0f;
    float time_since_teleport = 0.0f;
    bool is_switch = false;
    std::optional<PushFootFrame> foot_frame;
};

class MotionHost : public s3::graph::controller::Host {
public:
    /// Every other field starts at its Rust `Default`; `from_graph` builds the stock host.
    explicit MotionHost(MotionAnimation animation_) : animation(std::move(animation_)) {}

    MotionAnimation animation;
    PlaybackContext playback_context;
    ConditionInputs condition_inputs;
    std::optional<motion_gameplay_conditions::GameplayConditions> gameplay_conditions;
    std::optional<bool> manual_exit;
    /// Completed Skeleton PhysOut536/540, in native yaw-then-pitch order.
    std::optional<std::array<float, 2>> deck_yaw_pitch;
    std::optional<motion_riding_conditions::RidingConditionInputs> riding_conditions;
    motion_riding_conditions::MotionRandom condition_random;
    std::optional<motion_native::Physical> native_physical;
    std::optional<motion_spin::PrelandingPhysical> prelanding_physical;
    std::optional<s3::animation::air_leg_extension::Physical> air_leg_physical;
    std::optional<motion_native::GesturePhysical> gesture_physical;
    std::optional<motion_character_gesture::GesturePublication> gesture_publication;
    std::optional<motion_toggle_board::Physical> toggle_board_physical;
    std::optional<motion_runout::Observation> runout_physical;
    std::optional<motion_grind::Physical> grind_physical;
    std::optional<motion_grind::conditions::Physical> grind_conditions;
    /// `pub(super)` in Rust.
    motion_grind::Settings grind_settings;
    std::optional<motion_shove::ShovePhysical> shove_physical;
    motion_hand_services::HandServices hand_services;
    s3::input::set_turning::SlideLatch slide_latch;
    ///SpecificCA4 bit26, getter8258F900/setter8258F910; reset clears it.
    bool is_power_sliding = false;
    motion_native::ScorePacket score_packet;
    /// `pub(super)` in Rust.
    motion_tricks::Requests trick_requests;
    /// `pub(super)` in Rust.
    std::pair<bool, bool> trick_height_settings{true, true};
    ActionControls action_controls;
    /// `pub(crate)` in Rust.
    IntentMap action_intents;
    /// Typed turning values transported from the ActionGraph boundary.
    TurningOutput turning_output;
    GraphEffects graph_effects;
    std::optional<std::map<std::string, float>> time_tags;
    std::optional<MotionPhysical> physical;
    /// Completed Biped/OffBoard publications. These remain absent until the
    /// corresponding physical owner is integrated; graph leaves must not
    /// invent replacement values.
    std::optional<float> offboard_cadence_phase;
    std::optional<std::uint32_t> offboard_locomotion_state;
    std::optional<std::uint32_t> ground_slope_type;
    std::optional<bool> biped_ground_thin;
    float animation_phase = 0.0f;
    motion_stock_gameplay::MovingObjectRegistry moving_objects;
    std::optional<PushState> push_state;
    bool applying_body_tilt = false;
    std::optional<s3::animation::crouching::Physical> crouching_physical;
    std::optional<s3::animation::body_tilt::Physical> body_tilt_physical;
    bool hold_fakie = false;
    /// Private in Rust.
    std::vector<bool> automatic_fakie_conditions;
    std::optional<s3::animation::riding_fakie::Physical> fakie_physical;
    /// PhysOutAnimation158/157, used directly by IsRidingGoofy82BA5AA8.
    std::optional<std::pair<bool, bool>> physical_stance;
    motion_landing::Flags flags;
    /// Private in Rust.
    motion_hippy_jump::Settings hippy_jump;
    /// Private in Rust.
    motion_finger_flip::Settings finger_flip;
    std::optional<motion_landing::Physical> landing_physical;
    std::optional<motion_wipeout::Physical> wipeout_physical;
    motion_wipeout::Controls wipeout_controls;
    /// Private in Rust.
    motion_wipeout::Settings wipeout_settings;
    ///Native PhysOutGround pumping acceleration268.
    std::optional<float> pumping_acceleration;
    /// Completed PhysOutAnimation112 acceleration before bump conditioning.
    std::optional<std::array<float, 4>> bump_acceleration;
    /// Private in Rust.
    motion_bump::Settings bump_settings;
    bool allow_pumping = true;
    motion_riding::RidingState riding;
    GraphDiagnostics errors;
    /// `pub(super)` in Rust.
    std::vector<std::optional<std::size_t>> state_parents;
    // Private in Rust (kept accessible for the ported tests and sibling modules).
    std::vector<MotionOperation> operations;
    std::vector<Instance> instances;
    OperationRemap remap;
    PushingSettings pushing;
    s3::input::set_turning::Settings turning;
    s3::animation::crouching::Settings crouching;
    s3::animation::body_tilt::Settings body_tilt;
    s3::animation::pumping_channel::Settings pumping;
    motion_sliding::Settings sliding;
    /// `pub(super)` in Rust.
    motion_spin::Settings spin;
    s3::animation::air_leg_extension::Settings air_leg;
    s3::animation::kickturn::Settings kickturn;
    s3::animation::manual::Settings manual;
    std::uint32_t next_instance = 1;

    /// `pub(super)` in Rust.
    void end_gesture_channels();

    static rs::Result<MotionHost, std::string> from_graph(const LoadedGraph& graph, const s3::data::Collections& data,
                                                          s3::data::AnimationMetadata metadata,
                                                          PlaybackContext playback_context);

    void accept_action_graph(MotionGraphInput input);

    /// Private in Rust.
    void run(BehaviorId id, const Frame& frame, std::uint8_t phase);

    // motion/execution.cpp (`pub(super)` in Rust).
    rs::Result<rs::Unit, std::string> execute(BehaviorId behavior, const Frame& frame, std::uint8_t phase);
    // motion/air_execution.cpp
    rs::Result<rs::Unit, std::string> execute_air(BehaviorId behavior, MotionOperation operation, const Frame& frame,
                                                  std::uint8_t phase);
    // motion/filter_execution.cpp
    rs::Result<rs::Unit, std::string> execute_intent_filter(BehaviorId behavior,
                                                            motion_intent_filter::Operation operation,
                                                            const Frame& frame, std::uint8_t phase);
    // motion/score_execution.cpp
    rs::Result<bool, std::string> execute_score_or_bump(const MotionOperation& operation, std::uint8_t phase);
    // motion/slide_execution.cpp
    rs::Result<rs::Unit, std::string> execute_slide(BehaviorId behavior, motion_sliding::Operation operation,
                                                    const Frame& frame, std::uint8_t phase);
    /// Private in Rust.
    rs::Result<float, std::string> slide_direction() const;
    // motion/toggle_execution.cpp
    rs::Result<rs::Unit, std::string> execute_toggle_board(BehaviorId behavior, std::uint8_t phase);
    // motion/wipeout_execution.cpp
    rs::Result<rs::Unit, std::string> execute_wipeout(BehaviorId behavior, motion_wipeout::Operation operation,
                                                      std::uint8_t phase);
    rs::Result<rs::Unit, std::string> execute_twist_lean(BehaviorId behavior,
                                                         motion_twist_lean::Operation operation,
                                                         std::uint8_t phase);

    // impl ConditionHost for MotionHost
    std::uint32_t condition_activation(std::size_t condition, const Frame& frame) override;
    // impl Host for MotionHost
    std::array<std::uint32_t, 6> context() const override { return {}; }
    std::uint32_t allocate(BehaviorId behavior, const Frame& frame) override;
    void begin(BehaviorId id, std::array<std::uint32_t, 6> context, const Frame& frame) override;
    void update(BehaviorId id, std::array<std::uint32_t, 6> context, const Frame& frame) override;
    void end(BehaviorId id, std::array<std::uint32_t, 6> context, const Frame& frame) override;
    void hook(HookId hook, const Frame& frame) override;
    void release(std::uint32_t /*instance*/) override {}
};

}  // namespace s3::game::graph_host::motion
