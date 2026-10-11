// Port of simref sim/src/camera/settings.rs.
#include "s3/game/camera/settings.hpp"

#include "s3/game/camera/_result.hpp"

namespace s3::game::camera::settings {

namespace cam = s3::camera;

namespace {

rs::Result<point_graph::PointGraph<8>, std::string> curve(const Collections& data, const std::string& cls,
                                                          const std::string& key, const std::string& name) {
    S3_CAM_TRY_DATA(auto words, data.words<16>(cls, key, name));
    const auto values = rs::from_bits(words);
    for (float value : values) {
        if (!rs::is_finite(value)) {
            return rs::make_err("Non-finite camera curve " + cls + "/" + key + "/" + name);
        }
    }
    point_graph::PointGraph<8> out;
    out.x = rs::from_fn<8>([&](std::size_t i) { return values[i]; });
    out.y = rs::from_fn<8>([&](std::size_t i) { return values[i + 8]; });
    return out;
}

rs::Result<float, std::string> f(const Collections& data, const std::string& cls, const std::string& key,
                                 const std::string& field) {
    return from_data(data.float_(cls, key, field));
}

}  // namespace

rs::Result<cam::ManagerSettings, std::string> manager_settings(const Collections& data) {
    cam::ManagerSettings out;
    S3_CAM_TRY(RigSettingsSource rig, RigSettingsSource::load(data));
    out.rig = rig.bind();
    S3_CAM_TRY(out.orientation, orientation_settings(data));
    S3_CAM_TRY(out.framing, frame_settings(data));
    S3_CAM_TRY(out.drop, drop_settings(data));
    S3_CAM_TRY(out.look, look_settings(data));
    S3_CAM_TRY(out.shake, shake_settings(data));
    S3_CAM_TRY(out.steering_threshold, f(data, "camera", "dynamics", "TurningCentredDeadzone"));
    return out;
}

rs::Result<cam::CompassSettings, std::string> compass_settings(const Collections& data) {
    auto g = [&](const char* field) { return f(data, "camera_compass", "default", field); };
    cam::CompassSettings out;
    S3_CAM_TRY(out.heading_response, f(data, "camera", "dynamics", "HeadingChangeResponseSpeed"));
    S3_CAM_TRY(out.time_before_lineup, g("TimeBeforeAutoLineup"));
    S3_CAM_TRY(out.lineup_speed, g("AutoLineupSpeed"));
    S3_CAM_TRY(out.minimum_deadzone_speed, g("MinDeadzoneSpeed"));
    S3_CAM_TRY(out.maximum_deadzone_speed, g("MaxDeadzoneSpeed"));
    S3_CAM_TRY(out.maximum_deadzone_size, g("MaxDeadzoneSize"));
    S3_CAM_TRY(out.deadzone_smoothing, g("DeadzoneSizeSmoothing"));
    return out;
}

rs::Result<cam::DropSettings, std::string> drop_settings(const Collections& data) {
    // Runtime parameter652/656/660/668 correspond to settings object
    //668/672/676/684, bound from droppredictor layout208/204/220/228.
    cam::DropSettings out;
    S3_CAM_TRY(out.minimum_test_distance, f(data, "camera_droppredictor", "default", "MinTestDistance"));
    S3_CAM_TRY(out.total_test_time, f(data, "camera_droppredictor", "default", "TotalTestTime"));
    S3_CAM_TRY(out.maximum_test_distance, f(data, "camera_droppredictor", "default", "Hash_FB5A1024D47298A1"));
    S3_CAM_TRY(out.maximum_drop_distance, f(data, "camera_droppredictor", "default", "MaxDropDistance"));
    return out;
}

rs::Result<cam::LookSettings, std::string> look_settings(const Collections& data) {
    // Settings object952/960/964/972 -> parameter936/944/948/956.
    cam::LookSettings out;
    S3_CAM_TRY(out.heading_offset_degrees, f(data, "camera", "freecam", "FreeCamHeadingOffset"));
    S3_CAM_TRY(out.heading_speed, f(data, "camera", "freecam", "FreeCamHeadingSpeed"));
    S3_CAM_TRY(out.elevation_offset_degrees, f(data, "camera", "freecam", "FreeCamElevationOffset"));
    S3_CAM_TRY(out.elevation_speed, f(data, "camera", "freecam", "FreeCamElevationSpeed"));
    return out;
}

rs::Result<cam::ShakeSettings, std::string> shake_settings(const Collections& data) {
    auto g = [&](const char* field) { return f(data, "camera_shake", "default", field); };
    auto matrix = [&](const char* field) -> rs::Result<cam::F4x4, std::string> {
        S3_CAM_TRY_DATA(auto words, data.words<16>("camera_shake", "default", field));
        return rs::from_fn<4>([&](std::size_t row) {
            return rs::from_fn<4>([&](std::size_t i) { return rs::from_bits(words[row * 4 + i]); });
        });
    };
    cam::ShakeSettings out;
    S3_CAM_TRY(out.amplitude_curve, matrix("AmplitudeCurve"));
    S3_CAM_TRY(out.frequency_curve, matrix("FrequencyCurve"));
    S3_CAM_TRY(out.impulse_magnitude, g("OneShotMagnitude"));
    S3_CAM_TRY(out.impulse_minimum_velocity, g("OneShotMinVelocity"));
    S3_CAM_TRY(out.impulse_maximum_velocity, g("OneShotMaxVelocity"));
    S3_CAM_TRY(out.impulse_frequency, g("OneShotFrequency"));
    S3_CAM_TRY(out.impulse_decay, g("OneShotDecay"));
    S3_CAM_TRY(out.amplitude_minimum, g("AmplitudeMin"));
    S3_CAM_TRY(out.amplitude_maximum, g("AmplitudeMax"));
    S3_CAM_TRY(out.amplitude_top_speed, g("AmplitudeTopSkaterSpeed"));
    S3_CAM_TRY(out.frequency_minimum, g("FrequencyMin"));
    S3_CAM_TRY(out.frequency_maximum, g("FrequencyMax"));
    S3_CAM_TRY(out.frequency_top_speed, g("FrequencyTopSkaterSpeed"));
    S3_CAM_TRY(out.data_frames_per_second, g("DataFPS"));
    S3_CAM_TRY(out.translation_multiplier, g("TranslationMultiplier"));
    S3_CAM_TRY(out.rotation_multiplier, g("RotationMultiplier"));
    S3_CAM_TRY(out.dutch_multiplier, g("DutchMultiplier"));
    return out;
}

rs::Result<RigSettingsSource, std::string> RigSettingsSource::load(const Collections& data) {
    // 8252E6CC binds heading; 8252E680 binds elevation. These are
    // settings for the normal camera's angular trackers, in degrees.
    auto angle = [&](const char* key) -> rs::Result<cam::AngleTrackingSettings, std::string> {
        cam::AngleTrackingSettings out;
        S3_CAM_TRY(out.speed_clamp_degrees, f(data, "camera_tracker", key, "SpeedClamp"));
        S3_CAM_TRY(out.acceleration_min_degrees, f(data, "camera_tracker", key, "AccelerationClampMin"));
        S3_CAM_TRY(out.acceleration_max_degrees, f(data, "camera_tracker", key, "AccelerationClampMax"));
        S3_CAM_TRY(out.delta_umbra_degrees, f(data, "camera_tracker", key, "DeltaUmbra"));
        S3_CAM_TRY(out.delta_penumbra_degrees, f(data, "camera_tracker", key, "DeltaPenumbra"));
        return out;
    };
    RigSettingsSource s;
    S3_CAM_TRY(s.heading, angle("heading"));
    S3_CAM_TRY(s.elevation, angle("elevation"));
    S3_CAM_TRY(s.anchor.latch_curve, curve(data, "camera_lag_profile", "push", "Curve"));
    S3_CAM_TRY(s.anchor.input_curve, curve(data, "camera_lag_profile", "pump", "Curve"));
    S3_CAM_TRY(s.anchor.speed_clamp, f(data, "camera", "subject_tracker", "SubjectTrackerMaxSpeed"));
    S3_CAM_TRY(s.anchor.acceleration_clamp, f(data, "camera", "subject_tracker", "SubjectTrackerMaxAcceleration"));
    S3_CAM_TRY(s.anchor.latch_scale, f(data, "camera_lag_profile", "push", "MaxSmoothing"));
    S3_CAM_TRY(s.anchor.input_threshold, f(data, "camera", "dynamics", "PumpingMinAcceleration"));
    S3_CAM_TRY(s.anchor.input_scale, f(data, "camera_lag_profile", "pump", "MaxSmoothing"));
    S3_CAM_TRY(s.avoidance.horizon, f(data, "camera", "dynamics", "AvoidanceLookahead"));
    S3_CAM_TRY(s.avoidance.heading_smoothing, f(data, "camera", "dynamics", "AvoidanceHeadingSmoothing"));
    S3_CAM_TRY(s.avoidance.elevation_smoothing, f(data, "camera", "dynamics", "AvoidanceElevationSmoothing"));
    S3_CAM_TRY(s.avoidance.ease_out_time, f(data, "camera", "dynamics", "AvoidanceEaseOut"));
    S3_CAM_TRY(s.avoidance.radius_padding, f(data, "camera_positioner", "default", "AvoidanceCollisionOffset"));
    S3_CAM_TRY(s.positioning.normal.speed_clamp, f(data, "camera_tracker", "distance", "SpeedClamp"));
    S3_CAM_TRY(s.positioning.normal.smoothing, f(data, "camera_tracker", "distance", "SmoothingConstant"));
    S3_CAM_TRY(s.positioning.normal.acceleration_clamp, f(data, "camera_tracker", "distance", "AccelerationClampMax"));
    S3_CAM_TRY(s.positioning.alternate.speed_clamp, f(data, "camera_tracker", "distance", "FastSpeedClamp"));
    // Native layout +12 has no recovered spelling. Its complete
    // key is preserved by the stock collection conversion.
    S3_CAM_TRY(s.positioning.alternate.smoothing, f(data, "camera_tracker", "distance", "Hash_E2D9D0FAAB8CB7F3"));
    S3_CAM_TRY(s.positioning.alternate.acceleration_clamp,
               f(data, "camera_tracker", "distance", "FastAccelerationClampMax"));
    S3_CAM_TRY(s.positioning.minimum_distance, f(data, "camera_positioner", "default", "MinDistance"));
    S3_CAM_TRY(s.positioning.alternate_minimum_distance,
               f(data, "camera_positioner", "default", "MinDistanceDuringWipeout"));
    S3_CAM_TRY(s.collision_hold_duration, f(data, "camera", "dynamics", "CollisionInterpolationDuration"));
    return s;
}

cam::RigSettings RigSettingsSource::bind() const {
    cam::RigSettings out;
    out.heading = this->heading;
    out.elevation = this->elevation;
    out.anchor = this->anchor;
    out.avoidance = this->avoidance;
    out.positioning = this->positioning;
    out.collision_hold_duration = this->collision_hold_duration;
    // 82F826F8 loads82181A88 with lvlx128, splats lane0, and stores
    // all four lanes at830BD350. This is not mapped BSS zero data.
    const float t = rs::from_bits(0x358637bd);
    out.normalization_threshold = {t, t, t, t};
    return out;
}

rs::Result<cam::OrientationSettings, std::string> orientation_settings(const Collections& data) {
    auto tracker = [&](const char* key) -> rs::Result<cam::OrientationTrackerSettings, std::string> {
        cam::OrientationTrackerSettings out;
        S3_CAM_TRY(out.acceleration_min_degrees, f(data, "camera_tracker", key, "AccelerationClampMin"));
        S3_CAM_TRY(out.acceleration_max_degrees, f(data, "camera_tracker", key, "AccelerationClampMax"));
        // 8252F448/8252F508 attribute lookup low wordD1CA9582.
        S3_CAM_TRY(out.smoothing_min, f(data, "camera_tracker", key, "SmoothingMin"));
        S3_CAM_TRY(out.delta_umbra_degrees, f(data, "camera_tracker", key, "DeltaUmbra"));
        S3_CAM_TRY(out.delta_penumbra_degrees, f(data, "camera_tracker", key, "DeltaPenumbra"));
        return out;
    };
    cam::OrientationSettings out;
    S3_CAM_TRY(out.pan_umbra_degrees, f(data, "camera", "rotation", "PanUmbra"));
    S3_CAM_TRY(out.pan_penumbra_degrees, f(data, "camera", "rotation", "PanPenumbra"));
    S3_CAM_TRY(out.pan_smoothing_curve, curve(data, "camera", "rotation", "PanSmoothingCurve"));
    S3_CAM_TRY(out.pan_minimum_smoothing, f(data, "camera", "rotation", "PanMinSmoothing"));
    S3_CAM_TRY(out.pan, tracker("pan"));
    S3_CAM_TRY(out.tilt, tracker("tilt"));
    return out;
}

rs::Result<cam::FrameSettings, std::string> frame_settings(const Collections& data) {
    // 8252EE80/8252EE90 read camera/dynamics layout624/576 into
    // settings-object660/628, hence parameter644/612.
    cam::FrameSettings out;
    S3_CAM_TRY(out.maximum_pitch_degrees, f(data, "camera", "dynamics", "MaxPitch"));
    S3_CAM_TRY(out.roll_response, f(data, "camera", "dynamics", "DutchInterpolationSpeed"));
    return out;
}

}  // namespace s3::game::camera::settings
