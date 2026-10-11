// Port of skate-core/src/player/offboard/toggle_board.rs.
// ToggleBoard TU3 82BA8FE8, retrieval 82BA9310, drop 82BA9990.
// Caller supplies completed physical observations and actual channel timing.
// Commands retain the native TransitionTo (+16) versus SequenceTo (+20) distinction.
#pragma once
#include <array>
#include <optional>
#include <variant>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::toggle_board {

enum class Phase {
    Idle,  // default
    RetrieveStart,
    RetrieveInto,
    RetrieveCycle,
    RetrieveOut,
    DropStart,
    DropPlaying,
};

struct Input {
    /// OffBoard304, OffBoard311, bundle28 byte87, OffBoard313.
    bool grabbing_object = false;
    bool holding_board = false;
    bool retrieval_blocked = false;
    bool retrieval_active = false;
    bool drop_requested = false;
    bool throw_requested = false;
    bool retrieve_requested = false;
    /// OffBoard36/40 and animation component virtual28.
    float yaw_radians = 0.0f;
    float pitch_radians = 0.0f;
    bool mirrored = false;
};

struct Channel {
    bool exists = false;
    float remaining = 0.0f;
    float elapsed = 0.0f;
};

enum class Clip { Throw, Drop, FrontInto, FrontCycle, FrontOut, BackInto, BackCycle, BackOut };
/// Constructor82BA8EC8; drop deliberately selects THROW_90.
inline const char* name(Clip self) {
    switch (self) {
        case Clip::Throw: return "OFB_THROW_0";
        case Clip::Drop: return "OFB_THROW_90";
        case Clip::FrontInto: return "OFB_RETRIEVE_HIGH_FRONT_INTO";
        case Clip::FrontCycle: return "OFB_RETRIEVE_HIGH_FRONT_CYC";
        case Clip::FrontOut: return "OFB_RETRIEVE_HIGH_FRONT_OUT";
        case Clip::BackInto: return "OFB_RETRIEVE_HIGH_BACK_INTO";
        case Clip::BackCycle: return "OFB_RETRIEVE_HIGH_BACK_CYC";
        case Clip::BackOut: return "OFB_RETRIEVE_HIGH_BACK_OUT";
    }
    return "";
}

struct CommandBlendTo {
    Clip _0{};
};
S3_DERIVE_EQ(CommandBlendTo, _0)
struct CommandSequenceTo {
    Clip _0{};
};
S3_DERIVE_EQ(CommandSequenceTo, _0)
struct CommandStop {
    float blend_seconds = 0.0f;
};
S3_DERIVE_EQ(CommandStop, blend_seconds)
/// Rust `enum Command { BlendTo(Clip), SequenceTo(Clip), Stop { blend_seconds } }`.
struct Command {
    std::variant<CommandBlendTo, CommandSequenceTo, CommandStop> value;
    static Command BlendTo(Clip c) { return Command{CommandBlendTo{c}}; }
    static Command SequenceTo(Clip c) { return Command{CommandSequenceTo{c}}; }
    static Command Stop(float blend_seconds) { return Command{CommandStop{blend_seconds}}; }
};
S3_DERIVE_EQ(Command, value)

struct Output {
    /// Append each true flag to MG physical attributes and playback parameters.
    bool retrieving = false;
    bool dropping = false;
    /// One-tick OB_RetrieveBoard publication on each cycle transition.
    bool retrieve = false;
    /// Retrieval branches publish yaw then pitch, even on the terminating tick.
    std::optional<std::array<float, 2>> yaw_pitch;
    std::optional<Command> channel;
};
S3_DERIVE_EQ(Output, retrieving, dropping, retrieve, yaw_pitch, channel)

class State {
public:
    Phase phase = Phase::Idle;

    /// Begin82BA8FD8 resets phase then tail-calls ResetYawPitch82BA9D08.
    void begin() {
        phase = Phase::Idle;
        reset_orientation();
    }
    Output update(const Input& input, const Channel& channel);

private:
    void reset_orientation() {
        previous_yaw_ = 0.0f;
        yaw_ = 0.0f;
        pitch_ = 0.0f;
        initialized_yaw_ = false;
        back_ = false;
    }
    /// 82BA9E20; five degrees per update, independent of delta time.
    void orientation(const Input& input);
    void cycle(Output& out);
    void stop(Output& out);

    float previous_yaw_ = 0.0f;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    bool initialized_yaw_ = false;
    bool back_ = false;
    bool throwing_ = false;
};

}  // namespace s3::player::offboard::toggle_board
