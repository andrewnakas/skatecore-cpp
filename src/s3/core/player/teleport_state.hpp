// Port of skate-core/src/player/teleport_state.rs (whole file).
// Teleporting702 from original TU3 VT823272FC.
// Enter82D431D8, Update82D431F0 and FillPhysOut82D43280.
#pragma once
#include <cstdint>
#include <optional>

#include "s3/core/player/input_phase/raw.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::teleport_state {

using input_phase::types::RawMatrix;

struct Target {
    RawMatrix transform{};
    bool on_board = false;
};
S3_DERIVE_EQ(Target, transform, on_board)

enum class Update {
    /// Skeleton16552 virtual36: ask the actor for a reset target.
    RequestCheckpoint,
    Captured,
};

struct Output {
    RawMatrix transform{};
    std::uint32_t next_state = 0;
    std::uint8_t state_61 = 0;
    std::uint8_t board_272 = 0;
};
S3_DERIVE_EQ(Output, transform, next_state, state_61, board_272)

class TeleportState {
public:
    /// Clears only the three lifecycle flags; the retained matrix is unread
    /// until Update captures a new target. Native Exit is an empty leaf.
    void enter() {
        ready_ = false;
        received_ = false;
        on_board_ = true;
    }
    Update update(std::uint32_t flags_2468, const RawMatrix& matrix_1536, std::uint8_t byte_1600) {
        if ((flags_2468 & 2u) == 0) {
            return Update::RequestCheckpoint;
        }
        received_ = true;
        target_ = matrix_1536;
        ready_ = true;
        // rlwimi82D43254 inserts only byte1600 bit0, not a nonzero test.
        on_board_ = (byte_1600 & 1u) != 0;
        return Update::Captured;
    }
    /// This publisher does not clear the output when no target is ready.
    std::optional<Output> output() const {
        if (!ready_) return std::nullopt;
        return Output{target_, on_board_ ? 100u : 500u, 1, 1};
    }

private:
    RawMatrix target_{};     // 48
    bool ready_ = false;     // 116 bit31
    bool received_ = false;  // 116 bit30; native serialization retains this bit.
    bool on_board_ = true;   // 116 bit29
};

}  // namespace s3::player::teleport_state
