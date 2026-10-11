// Port of simref sim/src/camera/subject.rs.
#include "s3/game/camera/subject.hpp"

namespace s3::game::camera::subject {

namespace cam = s3::camera;

SubjectPublisher SubjectPublisher::new_() {
    SubjectPublisher p;
    p.pose = cam::SubjectPosePublisher::new_();
    p.anchors = cam::Anchors::new_();
    p.compass = cam::Compass::new_();
    p.last_tick = std::nullopt;
    return p;
}

rs::Result<cam::ManagerSubject, std::string> SubjectPublisher::publish(CameraSubjectSnapshot input,
                                                                       const cam::CameraMan& manager,
                                                                       cam::CompassSettings settings) {
    if (this->last_tick.has_value() && input.tick <= *this->last_tick) {
        return rs::make_err("Camera subject publication is not monotonic: previous=" +
                            std::to_string(*this->last_tick) + ", current=" + std::to_string(input.tick));
    }
    this->last_tick = input.tick;
    const cam::PublishedSubjectPose pose_ = this->pose.publish(input.pose);
    input.subject.rig.transform = pose_.transform;
    input.subject.rig.skeleton_root = input.pose.skeleton_root;
    input.anchors.damped_center_of_mass = pose_.damped_center_of_mass;
    this->anchors.update(input.anchors);
    input.subject.anchors = this->anchors.entries;
    input.reference_points.damped_centre_of_mass = pose_.damped_center_of_mass;
    input.reference_points.tracked_anchor = manager.rig.anchor.position;
    input.reference_points.incline_normal = manager.state.incline_normal;
    input.subject.rig.reference_positions = input.reference_points.positions();
    const std::uint32_t selected =
        manager.shots.current().name.empty() ? 5u : manager.shots.current().shot.compass_north;
    const cam::CompassInputs compass_input = input.compass.bind(input.subject, manager.frame.position, selected);
    input.subject.compass =
        this->compass.update(input.subject.reset != 0 ? 0.0f : rs::from_bits(0x3c888889), compass_input, settings);
    return input.subject;
}

}  // namespace s3::game::camera::subject
