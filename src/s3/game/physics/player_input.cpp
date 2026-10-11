// Port of skate-game/src/physics/player_input/mod.rs.
//! Concrete original Skate3 player input phase for the authored riding world.
#include "s3/game/physics/player_input.hpp"

#include "s3/game/physics/riding_outputs.hpp"

namespace s3::game::physics::player_input {

namespace input_phase = sc::player::input_phase;
using sc::math::Vector3;

namespace {
/// Rust `{:?}` of a String (quotes plus escapes).
std::string debug_string(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    out.push_back('"');
    return out;
}
/// Rust `{:?}` of `InputPhaseError<String>`.
std::string debug_error(const input_phase::InputPhaseError<std::string>& e) {
    if (e.value.index() == 0) {
        return "Service(" + debug_string(std::get<0>(e.value)._0) + ")";
    }
    return fmt::cat("InvalidStateVariant(", std::get<1>(e.value)._0, ")");
}
}  // namespace

Status PlayerInputRuntime::request_teleport(AnimationPartTransform target) {
    if (pending_teleport_.has_value()) {
        return rs::make_err(std::string("A pending teleport must complete before replacement"));
    }
    pending_teleport_ = target;
    return ok();
}

rs::Result<PlayerInputRuntime, std::string> PlayerInputRuntime::load(const sd::Collections& data) {
    input_phase::PhysicalPlayerInput physical{};
    reset_outputs(physical);
    input_phase::ProcessedPhysicsInput processed{};
    reset::reset_processed(processed);
    PlayerInputRuntime out;
    S3G_TRY(out.player, initial::player(data));
    out.physical = physical;
    out.processed = processed;
    out.toolkit = std::nullopt;
    out.dynamic_normal = sc::physics::board_dynamic_normal::BoardDynamicNormal::new_();
    S3G_TRY(out.normal_settings, dynamic_normal::settings(data));
    out.pre_input = pre_input::PreInputManager::new_();
    S3G_TRY(out.grind, grind::GrindInputState::load(data));
    out.pending_teleport_ = std::nullopt;
    out.pending_grind = std::nullopt;
    out.grind_observation = std::nullopt;
    return out;
}

input_phase::ProcessedPhysicsSnapshot PlayerInputRuntime::processed_snapshot(std::uint64_t tick) const {
    return input_phase::ProcessedPhysicsSnapshot::new_(tick, processed);
}

void PlayerInputRuntime::update_dynamic_normal(const riding_outputs::RidingOutputs& riding, Vector3 gravity) {
    dynamic_normal.update(riding.ground, gravity, processed.scalar_2656, normal_settings);
}

Status PlayerInputRuntime::publish_board(const riding_outputs::RidingOutputs& riding) {
    if (!toolkit.has_value()) {
        return rs::make_err(std::string("Board output requires the current input toolkit"));
    }
    const auto& t = *toolkit;
    sc::physics::board_input_output::BoardInputFrame frame{
        xyz(t.deck[2]),
        riding.reckoning.ground_normal,
        xyz(riding.reckoning_frames.ground[1]),
        dynamic_normal.normal,
        processed.flags_2476,
    };
    sc::physics::board_input_output::publish_board_input(physical, riding.motion, riding.ground, frame);
    // Board Fill82C03318 and common ProcessOutput82DB7598.
    const Vector3 normal = riding.ground.wheel_normal;
    physical.ground.vector_80 = rs::to_bits(std::array<float, 4>{normal.x, normal.y, normal.z, 0.0f});
    physical.ground.flag_273 = (processed.flags_2468 & 0x00100000u) != 0 ? 1 : 0;
    return ok();
}

rs::Result<std::optional<input_phase::InputContinuation>, std::string> PlayerInputRuntime::process_stage(
    sc::physics::board_runtime::BoardRuntime& board, ground_runtime::GroundRuntime& ground,
    AnimationPartTransform ground_frame, const input_phase::AnimationInputPacket& packet, InputHostFrame host,
    PlayerInputCallbacks& callbacks, InputStage stage, const sc::physics::board_world::BoardWorld& world,
    const ::s3::game::grind_world::StaticProvider& world_edges) {
    services::Services service(board, ground, ground_frame, host, callbacks, pre_input, grind, world_edges, world,
                               pending_grind, toolkit, pending_teleport_);
    if (std::holds_alternative<InputStage::ThroughTeleport>(stage.value)) {
        auto r = input_phase::start_input<std::string>(player, physical, packet, processed, service);
        if (r.is_err()) return rs::make_err(fmt::cat("Player input: ", debug_error(r.unwrap_err())));
        return std::optional<input_phase::InputContinuation>(r.unwrap());
    }
    const auto continuation = std::get<InputStage::AfterTeleport>(stage.value)._0;
    auto r = input_phase::finish_input<std::string>(continuation, player, physical, packet, processed, service);
    if (r.is_err()) return rs::make_err(fmt::cat("Player input: ", debug_error(r.unwrap_err())));
    return std::optional<input_phase::InputContinuation>{};
}

Vector3 xyz(std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); }

}  // namespace s3::game::physics::player_input
