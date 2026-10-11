// Port of skate-core/src/player/input_phase/runtime.rs (whole file).
// Ordered runtime of TU3 `PhysicalPlayerHiLOD::Input` (`0x82DB4048`).
#pragma once
#include <cstdint>
#include <variant>

#include "s3/core/player/input_phase/publication.hpp"
#include "s3/core/player/input_phase/types.hpp"

namespace s3::player::input_phase::runtime {

using types::AnimationInputPacket;
using types::PhysicalPlayerInput;
using types::PlayerInputState;
using types::ProcessedPhysicsInput;
using types::RawVector;

/// Mandatory boundaries called by TU3 `PhysicalPlayerHiLOD::Input`.
///
/// Inlined motion calculations execute directly in this module. Services own
/// only separate physical subsystems and host data access.
/// (Rust trait with associated `Error` -> abstract interface templated on E.)
template <class E>
struct InputPhaseServices {
    using Error = E;
    template <class T>
    using R = rs::Result<T, E>;
    virtual ~InputPhaseServices() = default;

    virtual R<rs::Unit> update_pre_input_manager_82d81610(PlayerInputState& player,
                                                          PhysicalPlayerInput& physical) = 0;
    virtual R<rs::Unit> reset_processed_input_82bf9ef0(ProcessedPhysicsInput& output) = 0;
    virtual R<std::uint32_t> actor_query_slot_56() = 0;
    virtual R<std::uint32_t> actor_query_slot_44() = 0;
    virtual R<rs::Unit> reset_player_probe_82d7a330(PlayerInputState& player, PhysicalPlayerInput& physical) = 0;
    virtual R<rs::Unit> check_teleport_82db88c8(PlayerInputState& player, PhysicalPlayerInput& physical,
                                                ProcessedPhysicsInput& output) = 0;
    virtual R<bool> actor_input_available_slot_4() = 0;
    virtual R<float> transition_action_825903c8() = 0;
    virtual R<RawVector> calculate_ground_position_82c02840(const PhysicalPlayerInput& physical) = 0;
    virtual R<rs::Unit> prepare_board_toolkit_82c013f0(PlayerInputState& player, PhysicalPlayerInput& physical,
                                                       ProcessedPhysicsInput& output) = 0;
    virtual R<rs::Unit> process_skeleton_82bd8918(const AnimationInputPacket& packet, PhysicalPlayerInput& physical,
                                                  ProcessedPhysicsInput& output) = 0;
    virtual R<rs::Unit> update_grind_manager_82d8a828(PlayerInputState& player, PhysicalPlayerInput& physical,
                                                      ProcessedPhysicsInput& output) = 0;
};

/// Rust `enum InputPhaseError<E> { Service(E), InvalidStateVariant(u32) }`.
template <class E>
struct InputPhaseError {
    struct Service {
        E _0;
    };
    /// Native dereferences a null +2548 pointer when this packet value is >4.
    struct InvalidStateVariant {
        std::uint32_t _0;
    };
    std::variant<Service, InvalidStateVariant> value;

    static InputPhaseError service(E e) { return InputPhaseError{Service{std::move(e)}}; }
    static InputPhaseError invalid_state_variant(std::uint32_t v) { return InputPhaseError{InvalidStateVariant{v}}; }
};
template <class E>
bool operator==(const InputPhaseError<E>& a, const InputPhaseError<E>& b) {
    if (a.value.index() != b.value.index()) return false;
    if (a.value.index() == 0) return std::get<0>(a.value)._0 == std::get<0>(b.value)._0;
    return std::get<1>(a.value)._0 == std::get<1>(b.value)._0;
}
template <class E>
bool operator!=(const InputPhaseError<E>& a, const InputPhaseError<E>& b) {
    return !(a == b);
}

/// Pre-reset observations retained by82DB4048 across CheckTeleport.
/// (Fields are module-private in Rust; treat them as opaque.)
struct InputContinuation {
    std::uint32_t captured_state = 0;
    std::uint32_t captured_category = 0;
};

namespace detail {
inline constexpr float FIXED_FRAME_STEP = rs::from_bits(0x3c888889u);
void update_signed_ground_time(PlayerInputState& player, const PhysicalPlayerInput& physical,
                               ProcessedPhysicsInput& output);
void update_time_on_ground(PlayerInputState& player, std::uint32_t captured_category,
                           ProcessedPhysicsInput& output);
void update_timers(PlayerInputState& player, const PhysicalPlayerInput& physical, ProcessedPhysicsInput& output);
void update_ground_history(PlayerInputState& player, std::uint32_t captured_category,
                           const ProcessedPhysicsInput& output);
void publish_ground_history(const PlayerInputState& player, ProcessedPhysicsInput& output);
void publish_post_skeleton(PlayerInputState& player, const PhysicalPlayerInput& physical,
                           ProcessedPhysicsInput& output);
std::uint32_t native_nonnegative_decrement(std::uint32_t value);
inline float xenon_fsel(float test, float nonnegative, float negative) {
    return test >= -0.0f ? nonnegative : negative;
}
}  // namespace detail

// Rust `?` on a service result: map the error into InputPhaseError::Service.
#define S3_INPUT_PHASE_TRY(var, expr)                                                   \
    auto var = (expr);                                                                  \
    if (var.is_err()) return rs::make_err(InputPhaseError<E>::service(var.unwrap_err()))

///82DB4048 through CheckTeleport82DB42DC. Complete the physical reset
///continuation before calling finish_input; captured fields must survive it.
template <class E>
rs::Result<InputContinuation, InputPhaseError<E>> start_input(PlayerInputState& player,
                                                              PhysicalPlayerInput& physical,
                                                              const AnimationInputPacket& packet,
                                                              ProcessedPhysicsInput& output,
                                                              InputPhaseServices<E>& services) {
    using namespace publication;
    const std::uint32_t captured_state = physical.state.state_16;
    player.manager_1856_counter_320 = detail::native_nonnegative_decrement(player.manager_1856_counter_320);
    S3_INPUT_PHASE_TRY(r0, services.update_pre_input_manager_82d81610(player, physical));
    player.manager_1852_flag_256 = 0;
    const std::uint32_t captured_category = physical.state.category_12;
    S3_INPUT_PHASE_TRY(r1, services.reset_processed_input_82bf9ef0(output));

    publish_player_flags(player, output);
    S3_INPUT_PHASE_TRY(q56, services.actor_query_slot_56());
    output.actor_query_2948 = q56.unwrap();
    S3_INPUT_PHASE_TRY(q44, services.actor_query_slot_44());
    output.actor_query_2952 = q44.unwrap();
    publish_force_braking(player, physical, packet, output);
    output.probe_1792 = player.probe;
    S3_INPUT_PHASE_TRY(r2, services.reset_player_probe_82d7a330(player, physical));
    publish_collision_prefix(player, physical, output);
    publish_animation_packet(packet, output);

    output.state_identifier_2496 = physical.state.identifier_8;
    S3_INPUT_PHASE_TRY(r3, services.check_teleport_82db88c8(player, physical, output));
    return InputContinuation{captured_state, captured_category};
}

namespace detail {
template <class E>
rs::Result<rs::Unit, InputPhaseError<E>> publish_transition(const AnimationInputPacket& packet,
                                                            ProcessedPhysicsInput& output,
                                                            InputPhaseServices<E>& services) {
    S3_INPUT_PHASE_TRY(available, services.actor_input_available_slot_4());
    if (!available.unwrap()) {
        return rs::Unit{};
    }
    if (packet.suppress_transition_10376 != 0) {
        output.transition_2636 = 0.0f;
    } else {
        S3_INPUT_PHASE_TRY(action, services.transition_action_825903c8());
        const float value = action.unwrap();
        const float lower = xenon_fsel(-1.0f - value, -1.0f, value);
        output.transition_2636 = xenon_fsel(1.0f - lower, lower, 1.0f);
    }
    return rs::Unit{};
}

template <class E>
rs::Result<rs::Unit, InputPhaseError<E>> update_ground_position(PlayerInputState& player,
                                                                const PhysicalPlayerInput& physical,
                                                                std::uint32_t captured_category,
                                                                ProcessedPhysicsInput& output,
                                                                InputPhaseServices<E>& services) {
    if (captured_category == 500) {
        player.current_ground_position_1216 = physical.skeleton.anim_to_world_11920[3];
        player.time_off_board_1368 += FIXED_FRAME_STEP;
    } else {
        S3_INPUT_PHASE_TRY(position, services.calculate_ground_position_82c02840(physical));
        player.current_ground_position_1216 = position.unwrap();
        player.time_off_board_1368 = 0.0f;
    }
    output.time_off_board_2836 = player.time_off_board_1368;
    return rs::Unit{};
}
}  // namespace detail

///82DB42F0 through82DB5570, after any reset/normal-setup continuation.
template <class E>
rs::Result<rs::Unit, InputPhaseError<E>> finish_input(InputContinuation continuation, PlayerInputState& player,
                                                      PhysicalPlayerInput& physical,
                                                      const AnimationInputPacket& packet,
                                                      ProcessedPhysicsInput& output,
                                                      InputPhaseServices<E>& services) {
    using namespace publication;
    const std::uint32_t captured_state = continuation.captured_state;
    const std::uint32_t captured_category = continuation.captured_category;
    transfer_bit(output.flags_2472, 10, player.flags_1296, 29);
    {
        auto r = detail::publish_transition(packet, output, services);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
    }
    detail::update_signed_ground_time(player, physical, output);
    detail::update_time_on_ground(player, captured_category, output);

    {
        auto r = select_surface(player, packet, physical.surface_default_mode, output);
        if (r.is_err()) return rs::make_err(InputPhaseError<E>::invalid_state_variant(r.unwrap_err()));
    }
    replace_byte(output.flags_2476, 2, packet.flag_10370);
    publish_external_physics(player, packet, output);
    publish_state_prefix(player, physical, packet, captured_state, output);
    publish_physical_outputs(physical, packet, output);
    detail::update_timers(player, physical, output);
    {
        auto r = detail::update_ground_position(player, physical, captured_category, output, services);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
    }

    output.effective_anim_transform_192 =
        effective_animation_transform(physical.skeleton.anim_to_world_11920, output.flags_2476);
    detail::update_ground_history(player, captured_category, output);
    detail::publish_ground_history(player, output);
    S3_INPUT_PHASE_TRY(r0, services.prepare_board_toolkit_82c013f0(player, physical, output));
    publish_line_tests(player, output);
    S3_INPUT_PHASE_TRY(r1, services.process_skeleton_82bd8918(packet, physical, output));
    detail::publish_post_skeleton(player, physical, output);
    S3_INPUT_PHASE_TRY(r2, services.update_grind_manager_82d8a828(player, physical, output));
    return rs::Unit{};
}

/// Complete recovered TU3 PhysicalPlayerHiLOD::Input82DB4048.
/// The split also lets a host release subsystem borrows at CheckTeleport.
template <class E>
rs::Result<rs::Unit, InputPhaseError<E>> process_input(PlayerInputState& player, PhysicalPlayerInput& physical,
                                                       const AnimationInputPacket& packet,
                                                       ProcessedPhysicsInput& output,
                                                       InputPhaseServices<E>& services) {
    auto continuation = start_input(player, physical, packet, output, services);
    if (continuation.is_err()) return rs::make_err(continuation.unwrap_err());
    return finish_input(continuation.unwrap(), player, physical, packet, output, services);
}

#undef S3_INPUT_PHASE_TRY

}  // namespace s3::player::input_phase::runtime
