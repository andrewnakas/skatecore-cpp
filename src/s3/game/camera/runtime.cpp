// Port of simref sim/src/camera/runtime.rs.
#include "s3/game/camera/runtime.hpp"

#include <sstream>

#include "s3/data/collections.hpp"
#include "s3/game/camera/_result.hpp"
#include "s3/game/camera/collision.hpp"
#include "s3/game/camera/settings.hpp"
#include "s3/game/camera/shake_data.hpp"

namespace s3::game::camera::runtime {

namespace cam = s3::camera;
using s3::data::Collections;
using s3::data::join_path;

namespace {

std::string debug_f4(const cam::F4& v) {
    std::ostringstream os;
    os << "[" << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << "]";
    return os.str();
}
std::string debug_f4x4(const cam::F4x4& m) {
    std::string s = "[";
    for (std::size_t i = 0; i < 4; ++i) s += (i ? ", " : "") + debug_f4(m[i]);
    return s + "]";
}

}  // namespace

rs::Result<CameraRuntime, std::string> CameraRuntime::load(const s3::data::Vfs& vfs, const std::string& root) {
    S3_CAM_TRY_DATA(Collections data, Collections::load(vfs, root));
    S3_CAM_TRY_DATA(auto words, data.words<32>("slowmotion_controller", "default", "timescale"));
    const auto values = rs::from_bits(words);
    cam::SlowMotionSettings slow_motion;
    slow_motion.timescale.x = rs::from_fn<16>([&](std::size_t i) { return values[i]; });
    slow_motion.timescale.y = rs::from_fn<16>([&](std::size_t i) { return values[i + 16]; });
    S3_CAM_TRY_DATA(slow_motion.fps_at_scale_one, data.float_("slowmotion_controller", "default", "fps_at_scale_one"));
    S3_CAM_TRY(graph::CameraGraph graph_,
               graph::CameraGraph::load(
                   vfs, join_path(root, "private/stock/data/script/camera/Default_cameragraph.stategraph"),
                   slow_motion));
    auto samples = [&](const std::string& name) -> rs::Result<cam::ShakeSamples, std::string> {
        const std::string path = join_path(join_path(root, "private/stock/data/camera"), name);
        auto text = vfs.read_to_string(path);
        if (!text.ok) {
            return rs::make_err(path + ": " + text.error);
        }
        return shake_data::parse(text.value);
    };
    CameraRuntime out;
    out.manager = cam::CameraMan::new_();
    out.subject = subject::SubjectPublisher::new_();
    out.graph = std::move(graph_);
    S3_CAM_TRY(out.shots, shot_data::StockShots::from_collections(data));
    S3_CAM_TRY(out.settings, settings::manager_settings(data));
    S3_CAM_TRY(out.compass_settings, settings::compass_settings(data));
    S3_CAM_TRY(out.shakes[0], samples("1.shk"));
    S3_CAM_TRY(out.shakes[1], samples("2.shk"));
    out.trajectories = {trajectory::TrajectoryResult::new_(), trajectory::TrajectoryResult::new_(),
                        trajectory::TrajectoryResult::new_()};
    out.frame = std::nullopt;
    out.latest_subject = std::nullopt;
    out.simulation_rate_requests.clear();
    return out;
}

void CameraRuntime::set_aspect_ratio(float value) {
    // The frame check below rejects a non-finite field of view by returning
    // an error, which ends the run. A zero-area window -- minimised, or
    // mid-resize -- makes `width / height` NaN at the call site, so without
    // this the camera state is poisoned and the game exits. Holding the last
    // good ratio costs nothing: there is no frame to compose at zero area.
    if (rs::is_finite(value) && value > 0.0f) {
        this->manager.state.aspect_ratio = value;
    }
}

rs::Result<cam::CameraFrame, std::string> CameraRuntime::advance(float dt, const CameraSubjectSnapshot& snapshot,
                                                                 const s3::physics::board_world::BoardWorld& world,
                                                                 cam::F4 query_gravity,
                                                                 const CameraGraphEnvironment& environment,
                                                                 cam::MovingObstacleProvider& moving) {
    if (this->latest_subject.has_value() && snapshot.tick <= this->latest_subject->tick) {
        return rs::make_err("Camera received non-monotonic subject tick: previous=" +
                            std::to_string(this->latest_subject->tick) + ", current=" + std::to_string(snapshot.tick));
    }
    this->latest_subject = snapshot;
    S3_CAM_TRY(cam::ManagerSubject subject_,
               this->subject.publish(snapshot, this->manager, this->compass_settings));
    this->manager.prepare(subject_, this->settings);
    S3_CAM_TRY(auto requests,
               this->graph.update(dt, this->manager, subject_, snapshot.graph, environment, this->shots));
    this->simulation_rate_requests.insert(this->simulation_rate_requests.end(), requests.begin(), requests.end());
    trajectory::CameraTrajectory t0(world, query_gravity, this->trajectories[0]);
    trajectory::CameraTrajectory t1(world, query_gravity, this->trajectories[1]);
    trajectory::CameraTrajectory t2(world, query_gravity, this->trajectories[2]);
    std::array<cam::TrajectoryCollisionRequest*, 3> trajectories_ = {&t0, &t1, &t2};
    collision::CameraCollision collision_ = collision::CameraCollision::new_(world);
    S3_CAM_TRY(cam::CameraFrame frame_,
               this->manager.update(dt, subject_, this->settings, {&this->shakes[0], &this->shakes[1]}, trajectories_,
                                    moving, collision_));
    if (collision_.error.has_value()) {
        return rs::make_err(*collision_.error);
    }
    for (const auto& v : this->trajectories) {
        if (v.error.has_value()) {
            return rs::make_err(*v.error);
        }
    }
    bool finite = rs::is_finite(frame_.position[0]) && rs::is_finite(frame_.position[1]) &&
                  rs::is_finite(frame_.position[2]) && rs::is_finite(frame_.position[3]);
    for (const auto& column : frame_.basis.columns) {
        for (float v : column) finite = finite && rs::is_finite(v);
    }
    finite = finite && rs::is_finite(frame_.field_of_view_degrees);
    if (!finite) {
        std::ostringstream os;
        os << "Normal gameplay camera produced a non-finite frame: position=" << debug_f4(frame_.position)
           << " field_of_view=" << frame_.field_of_view_degrees
           << "; lens_length=" << this->manager.shots.interpolated.lens_length
           << "; aspect_ratio=" << this->manager.state.aspect_ratio
           << "; subject_transform=" << debug_f4x4(subject_.rig.transform)
           << "; skeleton_root=" << debug_f4x4(subject_.rig.skeleton_root)
           << "; ground_normal=" << debug_f4(subject_.ground_normal)
           << "; launch_position=" << debug_f4(subject_.launch_position)
           << "; landing_position=" << debug_f4(subject_.landing_position);
        return rs::make_err(os.str());
    }
    this->frame = frame_;
    return frame_;
}

}  // namespace s3::game::camera::runtime
