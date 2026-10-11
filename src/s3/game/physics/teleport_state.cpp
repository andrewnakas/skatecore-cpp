// Port of skate-game/src/physics/teleport_state.rs.
//! Host checkpoint response to original actor reset callback82592518.
//! Shared scheduling preserves request -> next input reply ->702 output -> reset.
#include "s3/game/physics/teleport_state.hpp"

namespace s3::game::physics::teleport_state {

namespace {
std::array<std::array<std::uint32_t, 4>, 4> bits(const AnimationPartTransform& t) {
    std::array<std::array<std::uint32_t, 4>, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::to_bits(t[i]);
    return out;
}
}  // namespace

void Runtime::request_manual(AnimationPartTransform transform, bool on_board) {
    vehicle_ejection = std::nullopt;
    pending_reply = Target{bits(transform), on_board};
    manual_on_board = on_board;
}

void Runtime::request_vehicle_ejection(AnimationPartTransform transform, std::array<float, 3> velocity,
                                       std::array<float, 3> angular) {
    request_manual(transform, false);
    vehicle_ejection = std::make_pair(velocity, angular);
}

std::optional<std::pair<std::array<float, 3>, std::array<float, 3>>> Runtime::take_vehicle_ejection() {
    auto out = vehicle_ejection;
    vehicle_ejection = std::nullopt;
    return out;
}

std::optional<bool> Runtime::take_manual_on_board() {
    auto out = manual_on_board;
    manual_on_board = std::nullopt;
    return out;
}

void Runtime::set_checkpoint(Checkpoint checkpoint_) {
    checkpoint = checkpoint_;
    pending_reply = std::nullopt;
}

Runtime Runtime::new_(Checkpoint checkpoint) {
    Runtime out;
    out.state = TeleportState{};
    out.checkpoint = checkpoint;
    return out;
}

void Runtime::enter() { state.enter(); }

bool Runtime::update(const sc::player::input_phase::ProcessedPhysicsInput& input) {
    return state.update(input.flags_2468, input.matrix_1536, input.byte_1600) == Update::RequestCheckpoint;
}

void Runtime::request_checkpoint() { reply(checkpoint); }

void Runtime::reply(Checkpoint c) { pending_reply = Target{bits(c.transform), c.on_board}; }

std::optional<ExternalReset> Runtime::take_reply() {
    auto reply = pending_reply;
    pending_reply = std::nullopt;
    if (!reply) return std::nullopt;
    return ExternalReset{reply->transform, std::uint8_t(reply->on_board)};
}

std::optional<Output> Runtime::publish_output(sc::player::input_phase::PhysicalPlayerInput& physical) const {
    const auto output = state.output();
    if (!output) return std::nullopt;
    physical.teleport_output = *output;
    physical.state.identifier_8 = output->next_state;
    physical.state.flag_61 = output->state_61;
    return output;
}

}  // namespace s3::game::physics::teleport_state
