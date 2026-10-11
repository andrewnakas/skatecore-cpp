// Port of skate-core/src/player/tests/frame.rs.
#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "harness.hpp"
#include "s3/core/player/frame.hpp"

namespace {
using namespace s3::player::frame;
using s3::rs::Result;
using s3::rs::Unit;
using R = Result<Unit, FrameStage>;

struct Recorder {
    std::shared_ptr<std::vector<FrameStage>> calls;
    std::optional<FrameStage> fail;

    R call(FrameStage stage) {
        calls->push_back(stage);
        if (fail == stage) return s3::rs::make_err(stage);
        return Unit{};
    }
    R phase_one() { return call(FrameStage::AnimationPhaseOne); }
    R phase_two() { return call(FrameStage::AnimationPhaseTwo); }
    R phase_three() { return call(FrameStage::AnimationPhaseThree); }
    R publish_physics_input() { return call(FrameStage::ActorPhysicsPublication); }
    R start_frame(float) { return call(FrameStage::StartFrame); }
    R pre_frame() { return call(FrameStage::WorldPreFrame); }
    R prepare() { return call(FrameStage::WorldPrepare); }
    R precursor() { return call(FrameStage::WorldPrecursor); }
    R after_input() { return call(FrameStage::WorldAfterInput); }
    R collect_solver_inputs() { return call(FrameStage::CollectSolverInputs); }
    R finish_outputs() { return call(FrameStage::FinishOutputs); }
    R post_zero_jobs() { return call(FrameStage::PostZeroJobs); }
    R input() { return call(FrameStage::Input); }
    R post_input() { return call(FrameStage::PostInput); }
    R logic() { return call(FrameStage::Logic); }
    R pre_state() { return call(FrameStage::PreState); }
    R state() { return call(FrameStage::State); }
    R post_state() { return call(FrameStage::PostState); }
    R adjust() { return call(FrameStage::Adjust); }
    R post_adjust() { return call(FrameStage::PostAdjust); }
    R output() { return call(FrameStage::Output); }
    R post_output() { return call(FrameStage::PostOutput); }
    R prepare_jobs() { return call(FrameStage::PrepareSolverJobs); }
    R submit() { return call(FrameStage::SubmitSolver); }
    R wait() { return call(FrameStage::WaitSolver); }
};

std::pair<std::shared_ptr<std::vector<FrameStage>>, std::array<Recorder, 4>> recorders(std::optional<FrameStage> fail) {
    auto calls = std::make_shared<std::vector<FrameStage>>();
    Recorder r{calls, fail};
    return {calls, {r, r, r, r}};
}

S3_TEST(runs_the_complete_native_order_across_the_solver_fence) {
    auto [calls, rec] = recorders(std::nullopt);
    auto& [animation, world, physical, solver] = rec;
    FrameScheduler scheduler = FrameScheduler::default_();
    scheduler.run_before_solver<FrameStage>(1.0f / 60.0f, animation, world, physical, solver).unwrap();
    CHECK(scheduler.state() == SchedulerState::SolverPending);
    scheduler.run_after_solver<FrameStage>(world, physical, solver).unwrap();
    CHECK(scheduler.state() == SchedulerState::Ready);
    using F = FrameStage;
    const std::vector<FrameStage> expected{
        F::StartFrame,     F::WorldPreFrame,       F::AnimationPhaseOne,     F::AnimationPhaseTwo,
        F::WorldPrepare,   F::AnimationPhaseThree, F::WorldPrecursor,        F::ActorPhysicsPublication,
        F::PostZeroJobs,   F::Input,               F::WorldAfterInput,       F::PostInput,
        F::Logic,          F::PreState,            F::State,                 F::PostState,
        F::CollectSolverInputs, F::PrepareSolverJobs, F::SubmitSolver,      F::WaitSolver,
        F::Adjust,         F::PostAdjust,          F::Output,                F::PostOutput,
        F::FinishOutputs,
    };
    CHECK(*calls == expected);
}

S3_TEST(post_solver_work_cannot_run_before_submission) {
    auto [calls, rec] = recorders(std::nullopt);
    (void)calls;
    auto& [animation, world, physical, solver] = rec;
    (void)animation;
    FrameScheduler scheduler = FrameScheduler::default_();
    using SE = ScheduleError<FrameStage>;
    CHECK(scheduler.run_after_solver<FrameStage>(world, physical, solver) ==
          (Result<Unit, SE>(s3::rs::make_err(SE{SE::WrongState{SchedulerState::SolverPending, SchedulerState::Ready}}))));
}

S3_TEST(a_failed_phase_stops_the_frame_and_cannot_publish_later_output) {
    auto [calls, rec] = recorders(FrameStage::Logic);
    auto& [animation, world, physical, solver] = rec;
    FrameScheduler scheduler = FrameScheduler::default_();
    using SE = ScheduleError<FrameStage>;
    CHECK(scheduler.run_before_solver<FrameStage>(1.0f / 60.0f, animation, world, physical, solver) ==
          (Result<Unit, SE>(s3::rs::make_err(SE{SE::Runtime{FrameStage::Logic, FrameStage::Logic}}))));
    CHECK(scheduler.state() == SchedulerState::Faulted);
    CHECK(calls->back() == FrameStage::Logic);
    CHECK(std::find(calls->begin(), calls->end(), FrameStage::SubmitSolver) == calls->end());
}
}  // namespace
