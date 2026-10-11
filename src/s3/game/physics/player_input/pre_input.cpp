// Port of skate-game/src/physics/player_input/pre_input.rs.
//! Original82D81610 inactive-query branch and82D2DDD0 result reset. Pending
//! geometry is explicit; this owner never treats an unimplemented query as a miss.
#include "s3/game/physics/player_input/pre_input.hpp"

namespace s3::game::physics::player_input::pre_input {

PreInputResult PreInputResult::reset() {
    AnimationPartTransform frame = sc::physics::skeleton_animation_record::IDENTITY;
    frame[3] = {0.0f, 0.0f, 0.0f, 0.0f};
    PreInputResult r;
    r.position = {0.0f, 0.0f, 0.0f, 0.0f};
    r.normal = {0.0f, 1.0f, 0.0f, 0.0f};
    r.frame = frame;
    r.vector96 = {0.0f, 0.0f, 0.0f, 0.0f};
    r.vector112 = {0.0f, 1.0f, 0.0f, 0.0f};
    r.vector128 = {0.0f, 0.0f, 0.0f, 0.0f};
    r.vector144 = {0.0f, 0.0f, 0.0f, 0.0f};
    r.scalar160 = 0.0f;
    r.word164 = 0;
    r.scalar168 = rs::from_bits(0x501502f9u);
    r.scalar172 = rs::from_bits(0x501502f9u);
    r.word176 = 0;
    r.flags180 = 0;
    return r;
}

PreInputManager PreInputManager::new_() {
    // 82D81100 clears pending316; result reset called by ctor82D8108C.
    PreInputManager m;
    m.pending_geometry = false;
    m.result = PreInputResult::reset();
    m.result_counts = {0, 0, 0};
    return m;
}

Status PreInputManager::prepare(std::uint32_t& counter) {
    result = PreInputResult::reset();
    result_counts = {0, 0, 0};
    counter = 30;
    if (pending_geometry) {
        return rs::make_err(
            std::string("Pending82D811C8 trajectory batch requires its complete82D81610 geometry consumer"));
    }
    return ok();
}

}  // namespace s3::game::physics::player_input::pre_input
