// Port of simref sim/src/camera/settings.rs.
//! Stock normal-camera rig settings, bound by TU3 8252E288/8252EDD0.
//! Runtime offsets in camera kernels are relative to settings-object +16.
#pragma once
#include <string>

#include "s3/core/camera/manager.hpp"
#include "s3/core/camera/compass.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::camera::settings {

using s3::data::Collections;

struct RigSettingsSource {
    s3::camera::AngleTrackingSettings heading;
    s3::camera::AngleTrackingSettings elevation;
    s3::camera::AnchorTrackingSettings anchor;
    s3::camera::AvoidanceSettings avoidance;
    s3::camera::RigPositioningSettings positioning;
    float collision_hold_duration = 0.0f;

    static rs::Result<RigSettingsSource, std::string> load(const Collections& data);
    s3::camera::RigSettings bind() const;
};

rs::Result<s3::camera::ManagerSettings, std::string> manager_settings(const Collections& data);
rs::Result<s3::camera::CompassSettings, std::string> compass_settings(const Collections& data);
rs::Result<s3::camera::DropSettings, std::string> drop_settings(const Collections& data);
rs::Result<s3::camera::LookSettings, std::string> look_settings(const Collections& data);
rs::Result<s3::camera::ShakeSettings, std::string> shake_settings(const Collections& data);
rs::Result<s3::camera::OrientationSettings, std::string> orientation_settings(const Collections& data);
rs::Result<s3::camera::FrameSettings, std::string> frame_settings(const Collections& data);

}  // namespace s3::game::camera::settings
