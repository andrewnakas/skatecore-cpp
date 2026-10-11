// Port of skate-core/src/player/input_phase/publication.rs (whole file).
// Direct field publication helpers of TU3 82DB4048 (pub(crate) in Rust).
#pragma once
#include <cstdint>

#include "s3/core/player/input_phase/types.hpp"

namespace s3::player::input_phase::publication {

using types::AnimationInputPacket;
using types::PhysicalPlayerInput;
using types::PlayerInputState;
using types::ProcessedPhysicsInput;
using types::RawMatrix;

void publish_player_flags(PlayerInputState& player, ProcessedPhysicsInput& out);
void publish_force_braking(const PlayerInputState& player, const PhysicalPlayerInput& physical,
                           const AnimationInputPacket& packet, ProcessedPhysicsInput& out);
void publish_collision_prefix(const PlayerInputState& player, const PhysicalPlayerInput& physical,
                              ProcessedPhysicsInput& out);
void publish_animation_packet(const AnimationInputPacket& packet, ProcessedPhysicsInput& out);
/// Err(index) when the packet's state variant is >4 (native null +2548 dereference).
rs::Result<rs::Unit, std::uint32_t> select_surface(const PlayerInputState& player, const AnimationInputPacket& packet,
                                                   std::uint32_t surface_default_mode, ProcessedPhysicsInput& out);
void publish_external_physics(PlayerInputState& player, const AnimationInputPacket& packet,
                              ProcessedPhysicsInput& out);
void publish_state_prefix(PlayerInputState& player, const PhysicalPlayerInput& physical,
                          const AnimationInputPacket& packet, std::uint32_t captured_state,
                          ProcessedPhysicsInput& out);
void publish_physical_outputs(const PhysicalPlayerInput& physical, const AnimationInputPacket& packet,
                              ProcessedPhysicsInput& out);
RawMatrix effective_animation_transform(RawMatrix source, std::uint32_t flags_2476);
void publish_line_tests(const PlayerInputState& player, ProcessedPhysicsInput& out);

inline void replace_byte(std::uint32_t& word, std::uint32_t bit, std::uint8_t source) {
    word = (word & ~(1u << bit)) | (std::uint32_t(source & 1u) << bit);
}
inline void replace_bool(std::uint32_t& word, std::uint32_t bit, bool source) {
    word = (word & ~(1u << bit)) | (std::uint32_t(source) << bit);
}
inline void transfer_bit(std::uint32_t& word, std::uint32_t bit, std::uint32_t source, std::uint32_t source_bit) {
    word = (word & ~(1u << bit)) | (((source >> source_bit) & 1u) << bit);
}

}  // namespace s3::player::input_phase::publication
