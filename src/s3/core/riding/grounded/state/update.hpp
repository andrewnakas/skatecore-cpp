// Port of skate-core/src/riding/grounded/state/update.rs.
// `PhysState_PhysicsGround::Update` (`82D37C88`) call composition.
#pragma once
#include <tuple>

#include "s3/core/riding/grounded/state/contact_state.hpp"
#include "s3/core/riding/grounded/state/data.hpp"
#include "s3/core/riding/pumping/controller.hpp"
#include "s3/core/riding/pumping/settings.hpp"
#include "s3/core/riding/pumping/state.hpp"

namespace s3::riding::grounded::state::update {

using data::GroundContactHistory;
using data::GroundUpdateInput;
using data::PhysicsGroundState;
using pumping::controller::PumpingSample;
using pumping::settings::PumpingMode;
using pumping::settings::PumpingSettings;
using pumping::state::PumpingState;

enum class GroundUpdateStage {
    SetElapsedSkeletonFlag,
    UpdateContactState,
    LoadPumpingFrame,
    UpdatePumping,
    UpdateSkateboard,
    PredictFutureDeck,
    SetCollisionSkeletonFlag,
    SetGroundSkeletonFlag,
    UpdateSpecialState,
    UpdateTrajectory,
};

template <class E>
struct GroundUpdateError {
    GroundUpdateStage stage = GroundUpdateStage::SetElapsedSkeletonFlag;
    E source;

    bool operator==(const GroundUpdateError& o) const { return stage == o.stage && source == o.source; }
    bool operator!=(const GroundUpdateError& o) const { return !(*this == o); }
};

/// Rust trait `GroundUpdateServices` (duck-typed template parameter `S`).
/// Required engine-facing operations in the exact `82D37C88` order. This
/// trait has no default implementation because each call mutates stock-owned
/// objects that are outside `skate-core`.
/// Supertraits: PumpingGeometry<Error = UpdateError> and
/// GroundContactServices<Error = UpdateError, Handle = ContactHandle>; a C++
/// implementor defines `Error`, `Handle`, `UpdateError` and `ContactHandle`.
///   rs::Result<rs::Unit, UpdateError> set_elapsed_skeleton_flag();
///   /// Supplies the exact deck-angle result produced at `82D37D34..82D37E54`
///   /// together with the other direct Pumping inputs. Generic host angle math
///   /// is not an allowed implementation of this producer.
///   rs::Result<std::tuple<PumpingSettings, PumpingMode, PumpingSample>, UpdateError> pumping_frame();
///   rs::Result<rs::Unit, UpdateError> update_skateboard(PhysicsGroundState& state);
///   rs::Result<rs::Unit, UpdateError> predict_future_deck();
///   rs::Result<rs::Unit, UpdateError> set_collision_skeleton_flag();
///   rs::Result<rs::Unit, UpdateError> set_ground_skeleton_flag();
///   rs::Result<rs::Unit, UpdateError> update_special_state();
///   rs::Result<rs::Unit, UpdateError> update_trajectory();

namespace detail {
template <class T, class E>
rs::Result<T, GroundUpdateError<E>> call(GroundUpdateStage stage, rs::Result<T, E> result) {
    if (result.is_err()) return rs::make_err(GroundUpdateError<E>{stage, result.unwrap_err()});
    return result.unwrap();
}
}  // namespace detail

/// Runs the full recovered outer Ground update. State writes before a failing
/// service remain visible, matching the native non-transactional execution.
template <class S>
rs::Result<rs::Unit, GroundUpdateError<typename S::UpdateError>> update(
    PhysicsGroundState& state, PumpingState& pumping, GroundContactHistory<typename S::ContactHandle>& contacts,
    GroundUpdateInput input, S& services) {
#define S3_UPDATE_CALL(stage_, expr)                                       \
    do {                                                                   \
        auto s3_upd_r_ = detail::call(stage_, (expr));                     \
        if (s3_upd_r_.is_err()) return rs::make_err(s3_upd_r_.unwrap_err()); \
    } while (0)
    const auto signals = state.begin_update(input);
    if (signals.set_skeleton_flag_16505) {
        S3_UPDATE_CALL(GroundUpdateStage::SetElapsedSkeletonFlag, services.set_elapsed_skeleton_flag());
    }
    S3_UPDATE_CALL(GroundUpdateStage::UpdateContactState,
                   contact_state::update(contacts, input.contact_state, services));
    auto frame = detail::call(GroundUpdateStage::LoadPumpingFrame, services.pumping_frame());
    if (frame.is_err()) return rs::make_err(frame.unwrap_err());
    const PumpingSettings settings = std::get<0>(frame.unwrap());
    const PumpingMode mode = std::get<1>(frame.unwrap());
    const PumpingSample sample = std::get<2>(frame.unwrap());
    S3_UPDATE_CALL(GroundUpdateStage::UpdatePumping,
                   pumping::controller::update_ground(pumping, settings, mode, sample, services));
    S3_UPDATE_CALL(GroundUpdateStage::UpdateSkateboard, services.update_skateboard(state));
    S3_UPDATE_CALL(GroundUpdateStage::PredictFutureDeck, services.predict_future_deck());
    if (state.flag_2722) {
        S3_UPDATE_CALL(GroundUpdateStage::SetCollisionSkeletonFlag, services.set_collision_skeleton_flag());
    }
    S3_UPDATE_CALL(GroundUpdateStage::SetGroundSkeletonFlag, services.set_ground_skeleton_flag());
    state.finish_update(input.timestep_2604);
#undef S3_UPDATE_CALL
    if ((input.contact_state.flags_2476 & 0x00400000u) != 0) {
        return detail::call(GroundUpdateStage::UpdateSpecialState, services.update_special_state());
    }
    return detail::call(GroundUpdateStage::UpdateTrajectory, services.update_trajectory());
}

}  // namespace s3::riding::grounded::state::update
