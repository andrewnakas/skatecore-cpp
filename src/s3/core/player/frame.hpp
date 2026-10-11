// Port of skate-core/src/player/frame.rs (whole file).
// Stock frame order recovered from TU3 `0x82859E70`, `0x8275EB10` and
// `0x8275ED68`. The solver boundary intentionally splits the frame because
// the native game submits jobs before returning for the post-solver pass.
//
// The Rust phase traits (AnimationPhases, WorldPhases, PhysicalPhases,
// SolverPhases, each with `type Error = E`) are duck-typed template parameters
// here: every phase method returns `rs::Result<rs::Unit, E>`.
#pragma once
#include <cstdint>
#include <variant>

#include "s3/core/rs.hpp"

namespace s3::player::frame {

enum class FrameStage {
    StartFrame,
    WorldPreFrame,
    AnimationPhaseOne,
    AnimationPhaseTwo,
    WorldPrepare,
    AnimationPhaseThree,
    WorldPrecursor,
    ActorPhysicsPublication,
    PostZeroJobs,
    Input,
    WorldAfterInput,
    PostInput,
    Logic,
    PreState,
    State,
    PostState,
    CollectSolverInputs,
    PrepareSolverJobs,
    SubmitSolver,
    WaitSolver,
    Adjust,
    PostAdjust,
    Output,
    PostOutput,
    FinishOutputs,
};

enum class SchedulerState {
    Ready,
    Running,
    SolverPending,
    Faulted,
};

/// Rust `enum ScheduleError<E> { WrongState {..}, Runtime {..} }`.
template <class E>
struct ScheduleError {
    struct WrongState {
        SchedulerState expected;
        SchedulerState actual;
        bool operator==(const WrongState& o) const { return expected == o.expected && actual == o.actual; }
    };
    struct Runtime {
        FrameStage stage;
        E source;
        bool operator==(const Runtime& o) const { return stage == o.stage && source == o.source; }
    };
    std::variant<WrongState, Runtime> value;
    bool operator==(const ScheduleError& o) const { return value == o.value; }
    bool operator!=(const ScheduleError& o) const { return !(value == o.value); }
};

class FrameScheduler {
public:
    static FrameScheduler default_() { return FrameScheduler{}; }
    SchedulerState state() const { return state_; }

    /// Runs the complete pre-solver half of the stock player frame. Successful
    /// return means jobs were submitted and their inputs must remain alive.
    template <class E, class A, class W, class P, class S>
    rs::Result<rs::Unit, ScheduleError<E>> run_before_solver(float timestep, A& animation, W& world, P& physical,
                                                             S& solver) {
        using R = rs::Result<rs::Unit, ScheduleError<E>>;
#define S3_FRAME_CALL(stage, expr)                     \
    do {                                               \
        auto r_ = call<E>(FrameStage::stage, (expr));  \
        if (r_.is_err()) return R(rs::make_err(r_.unwrap_err())); \
    } while (0)
        {
            auto r = require<E>(SchedulerState::Ready);
            if (r.is_err()) return r;
        }
        state_ = SchedulerState::Running;
        S3_FRAME_CALL(StartFrame, world.start_frame(timestep));
        S3_FRAME_CALL(WorldPreFrame, world.pre_frame());
        S3_FRAME_CALL(AnimationPhaseOne, animation.phase_one());
        S3_FRAME_CALL(AnimationPhaseTwo, animation.phase_two());
        S3_FRAME_CALL(WorldPrepare, world.prepare());
        S3_FRAME_CALL(AnimationPhaseThree, animation.phase_three());
        S3_FRAME_CALL(WorldPrecursor, world.precursor());
        S3_FRAME_CALL(ActorPhysicsPublication, animation.publish_physics_input());
        S3_FRAME_CALL(PostZeroJobs, physical.post_zero_jobs());
        S3_FRAME_CALL(Input, physical.input());
        S3_FRAME_CALL(WorldAfterInput, world.after_input());
        S3_FRAME_CALL(PostInput, physical.post_input());
        S3_FRAME_CALL(Logic, physical.logic());
        S3_FRAME_CALL(PreState, physical.pre_state());
        S3_FRAME_CALL(State, physical.state());
        S3_FRAME_CALL(PostState, physical.post_state());
        S3_FRAME_CALL(CollectSolverInputs, world.collect_solver_inputs());
        S3_FRAME_CALL(PrepareSolverJobs, solver.prepare_jobs());
        S3_FRAME_CALL(SubmitSolver, solver.submit());
        state_ = SchedulerState::SolverPending;
        return R(rs::Unit{});
    }

    /// Completes the stock post-solver half. The scheduler becomes reusable
    /// only after every output/conditioner phase succeeds.
    template <class E, class W, class P, class S>
    rs::Result<rs::Unit, ScheduleError<E>> run_after_solver(W& world, P& physical, S& solver) {
        using R = rs::Result<rs::Unit, ScheduleError<E>>;
        {
            auto r = require<E>(SchedulerState::SolverPending);
            if (r.is_err()) return r;
        }
        state_ = SchedulerState::Running;
        S3_FRAME_CALL(WaitSolver, solver.wait());
        S3_FRAME_CALL(Adjust, physical.adjust());
        S3_FRAME_CALL(PostAdjust, physical.post_adjust());
        S3_FRAME_CALL(Output, physical.output());
        S3_FRAME_CALL(PostOutput, physical.post_output());
        S3_FRAME_CALL(FinishOutputs, world.finish_outputs());
#undef S3_FRAME_CALL
        state_ = SchedulerState::Ready;
        return R(rs::Unit{});
    }

private:
    template <class E>
    rs::Result<rs::Unit, ScheduleError<E>> require(SchedulerState expected) const {
        if (state_ == expected) {
            return rs::Unit{};
        }
        return rs::make_err(ScheduleError<E>{typename ScheduleError<E>::WrongState{expected, state_}});
    }

    template <class E>
    rs::Result<rs::Unit, ScheduleError<E>> call(FrameStage stage, const rs::Result<rs::Unit, E>& result) {
        if (result.is_ok()) {
            return rs::Unit{};
        }
        state_ = SchedulerState::Faulted;
        return rs::make_err(ScheduleError<E>{typename ScheduleError<E>::Runtime{stage, result.unwrap_err()}});
    }

    SchedulerState state_ = SchedulerState::Ready;
};

}  // namespace s3::player::frame
