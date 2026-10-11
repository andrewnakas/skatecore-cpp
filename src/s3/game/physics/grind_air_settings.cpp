// Port of skate-game/src/physics/grind_air_settings.rs.
//! Stock GrindAirAdjust settings82D72B40; lengths from the actual deck/truck collections.
#include "s3/game/physics/grind_air_settings.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::game::physics::grind_air_settings {

using ::s3::physics::grind_air::Settings;

rs::Result<Settings, std::string> load(const sd::Collections& data) {
    auto air = [&](const char* key) { return data.float_("physics_grinds_air", "default", key); };
    auto deck = [&](const char* key) { return data.float_("physicsdeck", "default", key); };
    S3G_TRY(const float deck_mid, deck("DeckMidLength"));
    const float half = deck_mid * 0.5f;
    S3G_TRY(const float front_size, deck("DeckFrontEndSize"));
    S3G_TRY(const float tip_fraction, air("TipOffsetFraction"));
    const float tip = front_size * tip_fraction;
    S3G_TRY(const float front_angle, deck("DeckFrontEndAngle"));
    const float angle = front_angle * rs::f32::consts::PI / 180.0f;
    S3G_TRY(const float truck_z, data.float_("physicstrucks", "default", "TruckZPosFront"));
    const float z = half + truck_z;
    S3G_TRY(const float y, data.float_("physicstrucks", "default", "TruckYPos"));
    S3G_TRY(const std::uint32_t frames_raw, data.integer("physics_grinds_air", "default", "NumFramesToTest"));
    const std::size_t frames = std::size_t(frames_raw);
    if (!(frames >= 2 && frames <= 12)) {
        return rs::make_err(std::string("GrindAir NumFramesToTest exceeds native12-frame storage"));
    }
    Settings s;
    s.frames = frames;
    S3G_TRY(s.stomp, air("StompAccelerationAdjust"));
    const float tip_y = tip * ::s3::trigonometry::sin_cos(angle).first;
    s.points = {{
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, tip_y, half + tip, 0.0f},
        {0.0f, y, z, 0.0f},
        {0.0f, y, -z, 0.0f},
        {0.0f, tip_y, -half - tip, 0.0f},
    }};
    S3G_TRY(const float range_tip, air("AngleRangeTipSlide"));
    S3G_TRY(const float range_tip2, air("AngleRangeTipSlide"));
    S3G_TRY(const float range_board, air("AngleRangeBoardSlide"));
    S3G_TRY(const float range_5050, air("AngleRange50_50"));
    S3G_TRY(const float range_50, air("AngleRange5_0"));
    S3G_TRY(const float range_502, air("AngleRange5_0"));
    s.ranges = {90.0f, range_tip, range_tip2, range_board, range_5050, range_50, range_502};
    S3G_TRY(const float dist_tip, air("MaxDistTipSlide"));
    S3G_TRY(const float dist_tip2, air("MaxDistTipSlide"));
    S3G_TRY(const float dist_board, air("MaxDistBoardSlide"));
    S3G_TRY(const float dist_5050, air("MaxDist50_50"));
    S3G_TRY(const float dist_50, air("MaxDist5_0"));
    S3G_TRY(const float dist_502, air("MaxDist5_0"));
    s.distances = {z, z * dist_tip, z * dist_tip2, z * dist_board, z * dist_5050, z * dist_50, z * dist_502};
    S3G_TRY(const float yaw_board, air("AngleYAssistBoardSlide"));
    S3G_TRY(const float yaw_tip, air("AngleYAssistTipSlide"));
    S3G_TRY(const float yaw_tip2, air("AngleYAssistTipSlide"));
    S3G_TRY(const float yaw_board2, air("AngleYAssistBoardSlide"));
    S3G_TRY(const float yaw_5050, air("AngleYAssist50_50"));
    S3G_TRY(const float yaw_50, air("AngleYAssist5_0"));
    S3G_TRY(const float yaw_502, air("AngleYAssist5_0"));
    s.yaw_assist = {yaw_board, yaw_tip, yaw_tip2, yaw_board2, yaw_5050, yaw_50, yaw_502};
    S3G_TRY(s.max_offset, air("MaxOffsetDist"));
    S3G_TRY(s.max_delta, air("MaxOffsetDeltaPerFrame"));
    S3G_TRY(s.max_angle, air("MaxAngleDeltaPerFrame"));
    return s;
}

}  // namespace s3::game::physics::grind_air_settings
