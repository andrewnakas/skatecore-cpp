// Port of skate-core/src/player/post_input.rs (whole file).
// Complete player-owned portion of TU3 `PhysicalPlayerHiLOD::PostInput`
// (`0x82DB5588`) and its flag-latch helper (`0x82DB5CF8`).
//
// Grind, trajectory, the scalar calculation at `0x82DB5E10`, and candidate
// registration remain required services because they own separate systems.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::post_input {

struct PostInputPlayerFields {
    /// Player+1264, captured from PhysOut when its +442 byte is set.
    std::array<std::uint32_t, 4> jump_reference_1264{};
    /// Player+1296.
    std::uint32_t flags_1296 = 0;
    /// Player+1304.
    std::uint32_t state_frames_1304 = 0;
    /// Player+1308.
    std::uint32_t jump_fix_frames_1308 = 0;
    /// Player+1320, owned by `0x82DB5CF8`.
    std::uint32_t latch_frames_1320 = 0;
};
S3_DERIVE_EQ(PostInputPlayerFields, jump_reference_1264, flags_1296, state_frames_1304, jump_fix_frames_1308,
             latch_frames_1320)

struct PostInputProcessedFields {
    /// ProcessedPhysIn+848.
    std::array<std::uint32_t, 4> jump_reference_848{};
    std::uint32_t word_2464 = 0;
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t current_state_2508 = 0;
    std::uint32_t state_frames_2572 = 0;
    std::uint32_t jump_fix_frames_2576 = 0;
    float scalar_2740 = 0.0f;
};
S3_DERIVE_EQ(PostInputProcessedFields, jump_reference_848, word_2464, flags_2468, flags_2472, flags_2480, flags_2484,
             current_state_2508, state_frames_2572, jump_fix_frames_2576, scalar_2740)

struct PostInputPhysOutFields {
    /// PhysOut+32 -> +316.
    bool reset_state_frames_316 = false;
    /// PhysOut+8 -> +442.
    bool capture_jump_reference_442 = false;
    /// PhysOut+8 -> +128.
    std::array<std::uint32_t, 4> jump_reference_128{};
    /// PhysOut+76, set after every successful PostInput pass.
    bool complete_76 = false;
};
S3_DERIVE_EQ(PostInputPhysOutFields, reset_state_frames_316, capture_jump_reference_442, jump_reference_128,
             complete_76)

/// Player+1840 data consumed by `0x82D740F8`.
struct CandidatePublicationFields {
    bool first_object_present_196 = false;
    bool first_pending_288 = false;
    bool second_object_present_500 = false;
    bool second_pending_592 = false;
    std::uint32_t staged_word_12768 = 0;
    std::uint32_t staged_valid_12772 = 0;
    std::uint32_t staged_latched_12776 = 0;
    bool staged_pending_12780 = false;
};
S3_DERIVE_EQ(CandidatePublicationFields, first_object_present_196, first_pending_288, second_object_present_500,
             second_pending_592, staged_word_12768, staged_valid_12772, staged_latched_12776, staged_pending_12780)

enum class CandidateRegistration {
    /// `sub_82762AB0(ProcessedPhysIn+1888, component)`.
    First1888,
    /// `sub_82762AB0(ProcessedPhysIn+2176, component+304)`.
    Second2176,
};

/// Calls to independent systems, in their exact `0x82DB5588` order.
struct PostInputServices {
    virtual ~PostInputServices() = default;
    virtual void update_grind_manager_82d8ab08() = 0;
    virtual std::uint8_t update_trajectory_selector_82d68800() = 0;
    virtual float calculate_scalar_2740_82db5e10() = 0;
    virtual void register_candidate_82762ab0(CandidateRegistration registration) = 0;
};

struct PostInputContext {
    PostInputPlayerFields& player;
    PostInputProcessedFields& processed;
    PostInputPhysOutFields& phys_out;
    CandidatePublicationFields& candidates;
};

/// Runs the full TU3 PostInput phase for the fields owned by PhysicalPlayer.
void run_post_input(PostInputContext context, PostInputServices& services);

namespace detail {
/// Private helper (exposed for the ported tests): exact scalar bit transfers
/// and latch lifetime from TU3 `0x82DB5CF8`.
void update_flag_latches_82db5cf8(PostInputPlayerFields& player, PostInputProcessedFields& processed);
}  // namespace detail

///82762AB0 copies the defined fields, preserving destination padding and the
///low five bits of byte200. Words use native big-endian bit positions.
void copy_grab_record_82762ab0(std::array<std::uint32_t, 72>& destination, const std::array<std::uint32_t, 72>& source);

}  // namespace s3::player::post_input
