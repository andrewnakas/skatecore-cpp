// Port of skate-game/src/graph_host/motion_offboard/toggle_board.rs (module `motion_toggle_board`).
#include "s3/game/graph_host/motion_toggle_board.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::graph_host::motion_toggle_board {

using s3::animation::channel_playback::ChannelSettings;
using s3::animation::playback::TransitionSettings;
using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::skeleton_input::name::encode;
namespace toggle_board = s3::player::offboard::toggle_board;

rs::Result<rs::Unit, std::string> execute(State& state, MotionAnimation& animation, BoardControls actions,
                                          std::optional<Physical> physical, std::optional<bool> mirrored,
                                          std::uint8_t phase) {
    if (phase == 0) {
        state.begin();
        return rs::Unit{};
    }
    // End82B61BB8 does not stop the persistent channel.
    if (phase != 1) return rs::Unit{};
    if (!physical) {
        return rs::make_err(std::string(
            "ToggleBoard requires completed board-possession output (OffBoard36/40/304/311/313 and bundle28 byte87)"));
    }
    if (!mirrored) return rs::make_err(std::string("ToggleBoard requires the actual animation mirror state"));
    toggle_board::Input input;
    input.grabbing_object = physical->grabbing_object;
    input.holding_board = physical->holding_board;
    input.retrieval_blocked = physical->retrieval_blocked;
    input.retrieval_active = physical->retrieval_active;
    input.yaw_radians = physical->yaw_radians;
    input.pitch_radians = physical->pitch_radians;
    input.mirrored = *mirrored;
    input.drop_requested = actions.drop_requested;
    input.throw_requested = actions.throw_requested;
    input.retrieve_requested = actions.retrieve_requested;
    const toggle_board::Output output =
        state.update(input, toggle_board::Channel{animation.channels.has(CHANNEL), animation.channels.remaining(CHANNEL),
                                                  animation.channels.elapsed(CHANNEL)});
    // Original appends MG physical attributes before channel operations,
    // pulse after sequencing, and animation-only yaw/pitch last.
    if (output.retrieving) publish(animation, "OB_RetrievingBoard");
    if (output.dropping) publish(animation, "OB_DroppingBoard");
    if (output.channel) {
        if (const auto* stop = std::get_if<toggle_board::CommandStop>(&output.channel->value)) {
            animation.channels.end_with(CHANNEL, stop->blend_seconds, false);
        } else {
            const auto* blend = std::get_if<toggle_board::CommandBlendTo>(&output.channel->value);
            const bool sequence = blend == nullptr;
            const toggle_board::Clip clip =
                sequence ? std::get<toggle_board::CommandSequenceTo>(output.channel->value)._0 : blend->_0;
            const bool is_out = clip == toggle_board::Clip::FrontOut || clip == toggle_board::Clip::BackOut;
            const bool hold = sequence || is_out;
            ChannelSettings settings;
            settings.priority = 0;
            settings.keep_alive = false;
            settings.mirrored = false;
            settings.speed = 1.0f;
            settings.blend_in = 0.25f;
            settings.blend_out = 0.25f;
            settings.hold_during_blend_in = hold;
            settings.hold_during_blend_out = hold;
            settings.use_attributes = true;
            // Vtable8231E128: +16 = 82D1D090 (blend), +20 = 82D1D030
            // (sequence). Both preserve the channel owner via82D1D0F0.
            auto r = animation.transition_channel(
                CHANNEL, toggle_board::name(clip), settings,
                TransitionSettings{sequence ? 4u : 2u, sequence ? 0.0f : 0.25f, 0, 0, false}, true,
                !sequence && !is_out);
            if (r.is_err()) return rs::make_err(r.unwrap_err());
        }
    }
    if (output.retrieve) publish(animation, "OB_RetrieveBoard");
    if (output.yaw_pitch) {
        attribute(animation, "yaw", (*output.yaw_pitch)[0]);
        attribute(animation, "pitch", (*output.yaw_pitch)[1]);
    }
    return rs::Unit{};
}

void publish(MotionAnimation& animation, std::string_view name) {
    //82BA9434/9628: actor1800 interface slot16 ->8258F6E0 appends
    //a24-byte MG attribute; it does NOT insert a motion intention. Actor
    //82593760/70 copies that list into the actual physical input packet.
    animation.emit_packet(encode(name), 1.0f);
    //The second call (82BA9464/9658) separately sets the animation parameter.
    attribute(animation, name, 1.0f);
}

void attribute(MotionAnimation& animation, std::string_view name, float value) {
    animation.set_attribute(SettableAttribute{encode(name), value, false, -1});
}

}  // namespace s3::game::graph_host::motion_toggle_board
