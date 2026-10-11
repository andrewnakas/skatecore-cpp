// Port of skate-core/src/player/post_input.rs (whole file).
#include "s3/core/player/post_input.hpp"

#include <algorithm>

namespace s3::player::post_input {

namespace {
void replace_bit(std::uint32_t& word, std::uint32_t bit, std::uint32_t value) {
    word = (word & ~(1u << bit)) | ((value & 1u) << bit);
}

}  // namespace

namespace detail {
/// Exact scalar bit transfers and latch lifetime from TU3 `0x82DB5CF8`.
void update_flag_latches_82db5cf8(PostInputPlayerFields& player, PostInputProcessedFields& processed) {
    if ((processed.flags_2468 & 0x00200000u) != 0) {
        player.latch_frames_1320 = 0;
        player.flags_1296 |= 0x02000000u;
    }
    if ((processed.flags_2472 & 0x00000020u) != 0 || (processed.flags_2468 & 0x00400000u) != 0) {
        player.latch_frames_1320 = 0;
        player.flags_1296 |= 0x06000000u;
    }
    const std::uint32_t flags_before_expiry = player.flags_1296;
    if ((flags_before_expiry & 0x02000000u) != 0) {
        if ((processed.flags_2472 & 0x00000010u) != 0 ||
            ((flags_before_expiry & 0x08000000u) != 0 && (processed.flags_2472 & 0x00008000u) == 0)) {
            player.flags_1296 &= 0xF9FFFFFFu;
        }
        const std::uint32_t frames = player.latch_frames_1320;
        if (frames > 20 && (player.flags_1296 & 0x08000000u) == 0) {
            player.flags_1296 &= 0xF9FFFFFFu;
        }
        player.latch_frames_1320 = frames + 1u;
    }
    replace_bit(player.flags_1296, 28, (processed.flags_2472 >> 4) & 1u);
    replace_bit(player.flags_1296, 27, (processed.flags_2472 >> 15) & 1u);
    replace_bit(player.flags_1296, 17, (processed.flags_2480 >> 17) & 1u);
    replace_bit(processed.flags_2472, 3, (player.flags_1296 >> 26) & 1u);
    replace_bit(processed.flags_2472, 2, (player.flags_1296 >> 25) & 1u);
}

}  // namespace detail

namespace {
/// Complete TU3 `0x82D740F8` publication/reset behavior.
void publish_candidates_82d740f8(CandidatePublicationFields& fields, PostInputProcessedFields& processed,
                                 PostInputServices& services) {
    const bool first = fields.first_pending_288 && fields.first_object_present_196;
    if (first) {
        services.register_candidate_82762ab0(CandidateRegistration::First1888);
    }
    replace_bit(processed.flags_2480, 22, std::uint32_t(first));
    fields.first_pending_288 = false;

    const bool second = fields.second_pending_592 && fields.second_object_present_500;
    if (second) {
        services.register_candidate_82762ab0(CandidateRegistration::Second2176);
    }
    replace_bit(processed.flags_2480, 21, std::uint32_t(second));
    fields.second_pending_592 = false;

    if (fields.staged_pending_12780) {
        fields.staged_latched_12776 = 0;
        if (fields.staged_valid_12772 != 0) {
            fields.staged_latched_12776 = 1;
            processed.flags_2480 |= 0x00100000u;
            processed.word_2464 = fields.staged_word_12768;
        } else {
            processed.flags_2480 &= ~0x00100000u;
        }
    }
    fields.staged_pending_12780 = false;
}
}  // namespace

void run_post_input(PostInputContext context, PostInputServices& services) {
    auto& player = context.player;
    auto& processed = context.processed;
    auto& phys_out = context.phys_out;
    auto& candidates = context.candidates;

    services.update_grind_manager_82d8ab08();

    if (phys_out.reset_state_frames_316) {
        player.state_frames_1304 = 0;
    }
    player.state_frames_1304 = player.state_frames_1304 + 1u;

    const bool reset_state_frames =
        (processed.flags_2480 & 0x00002000u) != 0 ||
        ((processed.flags_2468 & 0x00400000u) != 0 && (player.flags_1296 & 0x40000000u) != 0);
    if (reset_state_frames) {
        player.state_frames_1304 = 0;
        player.flags_1296 &= ~0x40000000u;
    } else {
        replace_bit(player.flags_1296, 30, std::uint32_t((processed.flags_2468 & 0x00400000u) == 0));
        processed.flags_2468 &= ~0x00400000u;
    }
    processed.state_frames_2572 = player.state_frames_1304;

    if (phys_out.capture_jump_reference_442) {
        player.jump_fix_frames_1308 = 1;
        player.jump_reference_1264 = phys_out.jump_reference_128;
    } else {
        player.jump_fix_frames_1308 = player.jump_fix_frames_1308 + 1u;
    }
    processed.jump_fix_frames_2576 = player.jump_fix_frames_1308;
    processed.jump_reference_848 = player.jump_reference_1264;

    replace_bit(processed.flags_2468, 10, std::uint32_t(services.update_trajectory_selector_82d68800() & 1u));
    processed.scalar_2740 = services.calculate_scalar_2740_82db5e10();
    replace_bit(processed.flags_2484, 25, (player.flags_1296 >> 24) & 1u);

    if ((processed.flags_2484 & 0x00000800u) != 0) {
        player.flags_1296 |= 0x00200000u;
    } else if (processed.current_state_2508 == 103) {
        replace_bit(processed.flags_2484, 11, (player.flags_1296 >> 21) & 1u);
    } else {
        player.flags_1296 &= ~0x00200000u;
    }

    detail::update_flag_latches_82db5cf8(player, processed);
    publish_candidates_82d740f8(candidates, processed, services);
    phys_out.complete_76 = true;
}

void copy_grab_record_82762ab0(std::array<std::uint32_t, 72>& destination,
                               const std::array<std::uint32_t, 72>& source) {
    std::copy(source.begin(), source.begin() + 50, destination.begin());
    destination[50] = (destination[50] & 0x1fffffffu) | (source[50] & 0xe0000000u);
    std::copy(source.begin() + 51, source.begin() + 55, destination.begin() + 51);
    std::copy(source.begin() + 56, source.begin() + 66, destination.begin() + 56);
    destination[68] = source[68];
}

}  // namespace s3::player::post_input
