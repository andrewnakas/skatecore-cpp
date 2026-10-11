// Port of skate-game/src/physics/offboard/board_manager.rs
#include "s3/game/physics/offboard/board_manager.hpp"

namespace s3::game::physics::offboard::board_manager {

rs::Result<Owner, std::string> Owner::load(const sd::Collections& data) {
    S3G_TRY(Settings settings, board_possession::settings::load(data));
    return Owner{native::State{}, settings};
}

void Owner::update(SkateboardControllerFields& fields, const Observation& observation, Effects& effects) {
    state.update(fields, observation, settings, effects);
}

void Owner::hold(SkateboardControllerFields& fields, const Observation& observation, Effects& effects) {
    state.hold(fields, observation, effects);
}

void Owner::let_go(SkateboardControllerFields& fields, const Observation& observation, Effects& effects) {
    state.let_go(fields, observation, settings, effects);
}

void Owner::stop(SkateboardControllerFields& fields, const Observation& observation, Effects& effects) {
    state.stop(fields, observation, settings, effects);
}

void Owner::reset_for_teleport(SkateboardControllerFields& fields, const Observation& observation, Effects& effects) {
    fields.word_444 = 0;
    stop(fields, observation, effects);
    state.retrieval = native::Retrieval{};
}

void Owner::append_drives(BodySnapshot deck, std::array<BodySnapshot, 2> hands, std::size_t deck_reaction,
                          std::array<std::size_t, 2> hand_reactions, float dt,
                          std::vector<RetailDriveRows>& rows) const {
    board_possession::drives::append(state, deck, hands, deck_reaction, hand_reactions, dt, rows);
}

native::Fill Owner::fill(const SkateboardControllerFields& fields, const native::Processed& processed,
                         Frame bone_11) const {
    return native::fill(fields, state, processed, bone_11);
}

void Transition::finish(SkateboardControllerFields& fields) && {
    if (changed) fields.word_444 = previous.word_444;
}

void Transition::hold_skateboard() {
    previous.word_444 = 0;
    owner->hold(previous, *observation, *effects);
    changed = true;
}

void Transition::let_go_of_skateboard() {
    previous.word_444 = 0;
    owner->let_go(previous, *observation, *effects);
    changed = true;
}

}  // namespace s3::game::physics::offboard::board_manager
