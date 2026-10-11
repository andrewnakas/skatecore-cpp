// Port of skate-game/src/physics/wipeout_states/prediction.rs
//! Original Wipeout Update82D3C02C..C288; completed query precedes next submit.
#include "s3/game/physics/wipeout_states/prediction.hpp"

#include "s3/game/physics/air_trajectory.hpp"

namespace s3::game::physics::wipeout_states::prediction {

using ::s3::air::trajectory::QueryRequest;
using ::s3::air::trajectory::Trajectory;

void Prediction::reset() {
    pending.reset();
    result = ::s3::air::trajectory::QueryResult::miss();
}

Status Prediction::advance(::s3::player::wipeout_state::State& state, const ::s3::physics::board_world::BoardWorld& world,
                           std::array<float, 4> com_position, std::array<float, 4> gravity) {
    if (pending) {
        const auto [result_, trajectory, surface_query] = *pending;
        pending.reset();
        result = result_;
        if (surface_query) {
            if (!result_.valid() || (result_.surface & 0xF80u) != 0x600u) {
                state.below_surface = false;
                state.special_surface = false;
            }
        } else if (result_.valid()) {
            state.predicted_time = result_.contact_time;
            state.imminent_surface_twelve = (result_.surface & 0xF80u) == 0x600u && result_.contact_time < 0.1f;
            state.predicted_position = trajectory.position_at(result_.contact_time);
        } else {
            state.predicted_position = trajectory.position_at(3.0f);
            //The native miss branch preserves832 and573.
        }
    }
    const bool surface_query = state.below_surface;
    state.surface_query = surface_query;
    QueryRequest request{};
    if (surface_query) {
        auto position = com_position;
        position[1] = state.surface_height + 0.1f;
        request.trajectory = Trajectory{position, {0.0f, -0.2f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 1.0f};
        request.radius = 0.01f;
        request.start_error = 1.0f;
        request.end_error = 1.0f;
    } else {
        request.trajectory = Trajectory{com_position, state.velocity, gravity, 3.0f};
        request.radius = 0.3f;
        request.start_error = 1.0f;
        request.end_error = 1.0f;
    }
    S3G_TRY(const auto result_, air_trajectory::AirTrajectoryRuntime::query(world, request));
    //Host query execution is synchronous. The next tick's early control
    //must see this completed result, before consuming it at the update tail.
    result = result_;
    pending = std::make_tuple(result_, request.trajectory, surface_query);
    return ok();
}

}  // namespace s3::game::physics::wipeout_states::prediction
