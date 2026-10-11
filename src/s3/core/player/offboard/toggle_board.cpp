// Port of skate-core/src/player/offboard/toggle_board.rs.
#include "s3/core/player/offboard/toggle_board.hpp"

namespace s3::player::offboard::toggle_board {

void State::orientation(const Input& input) {
    float yaw = input.yaw_radians * -57.295776f;
    if (input.mirrored) yaw = -yaw;
    if (yaw < -90.0f) {
        if (initialized_yaw_) {
            yaw = yaw_ >= 0.0f ? 180.0f : -90.0f;
        } else {
            yaw = yaw >= -135.0f ? -90.0f : 180.0f;
        }
    }
    if (input.mirrored) yaw = -yaw;
    if (initialized_yaw_) {
        const float difference = yaw - previous_yaw_;
        // Preserve the original ordered fsel arithmetic, including NaNs.
        const float lower = -5.0f - difference >= 0.0f ? -5.0f : difference;
        const float step = 5.0f - lower >= 0.0f ? lower : 5.0f;
        yaw = step + previous_yaw_;
    }
    previous_yaw_ = yaw;
    yaw_ = input.mirrored ? -yaw : yaw;
    pitch_ = input.pitch_radians * 57.295776f;
    initialized_yaw_ = true;
    back_ = rs::abs(yaw_) > 90.0f;
}

Output State::update(const Input& input, const Channel& channel) {
    if (!input.grabbing_object && phase == Phase::Idle) {
        if (input.drop_requested && input.holding_board) {
            phase = Phase::DropStart;
            throwing_ = false;
        } else if (input.throw_requested && input.holding_board) {
            phase = Phase::DropStart;
            throwing_ = true;
        } else if (input.retrieve_requested && !input.holding_board && !input.retrieval_blocked) {
            phase = Phase::RetrieveStart;
        }
    }
    Output out{};
    switch (phase) {
        case Phase::Idle:
            reset_orientation();
            break;
        case Phase::DropStart:
            out.dropping = true;
            phase = Phase::DropPlaying;
            out.channel = Command::BlendTo(throwing_ ? Clip::Throw : Clip::Drop);
            break;
        case Phase::DropPlaying:
            out.dropping = true;
            if (!channel.exists || channel.remaining <= 0.0f || channel.elapsed > 0.5f) stop(out);
            break;
        default:
            out.retrieving = true;
            switch (phase) {
                case Phase::RetrieveStart:
                    orientation(input);
                    phase = Phase::RetrieveInto;
                    out.channel = Command::BlendTo(back_ ? Clip::BackInto : Clip::FrontInto);
                    break;
                case Phase::RetrieveInto:
                    orientation(input);
                    if (!channel.exists || channel.remaining <= 0.0f) {
                        phase = Phase::Idle;
                    } else if (channel.remaining <= 0.25f) {
                        cycle(out);
                    }
                    break;
                case Phase::RetrieveCycle:
                    orientation(input);
                    if (!input.retrieval_active) {
                        stop(out);
                    } else if (!channel.exists || channel.remaining <= 0.0f) {
                        phase = Phase::Idle;
                    } else if (input.holding_board) {
                        phase = Phase::RetrieveOut;
                        out.channel = Command::BlendTo(back_ ? Clip::BackOut : Clip::FrontOut);
                    } else if (channel.remaining <= 0.25f) {
                        cycle(out);
                    }
                    break;
                case Phase::RetrieveOut:
                    if (!channel.exists || channel.remaining <= 0.0f || channel.elapsed > 0.16666667f) stop(out);
                    break;
                default:
                    S3_PANIC("unreachable");
            }
            out.yaw_pitch = std::array<float, 2>{yaw_, pitch_};
            break;
    }
    return out;
}

void State::cycle(Output& out) {
    phase = Phase::RetrieveCycle;
    out.channel = Command::SequenceTo(back_ ? Clip::BackCycle : Clip::FrontCycle);
    out.retrieve = true;
}

void State::stop(Output& out) {
    phase = Phase::Idle;
    out.channel = Command::Stop(0.16666667f);
}

}  // namespace s3::player::offboard::toggle_board
