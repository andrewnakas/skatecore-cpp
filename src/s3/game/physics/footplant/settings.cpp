// Port of skate-game/src/physics/footplant/settings.rs.
//! Cached physics_footplantmanager layout used by82D6FD60/82D6F5F0/82D70070.
#include "s3/game/physics/footplant/settings.hpp"

namespace s3::game::physics::footplant::settings {

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data) {
    auto float_ = [&](const char* name) { return data.float_("physics_footplantmanager", "default", name); };
    S3G_TRY(const auto graph_words, data.words<20>("physics_footplantmanager", "default", "Hash_8A8887218466ED15"));
    const auto graph = rs::from_bits(graph_words);
    Settings s;
    S3G_TRY(const auto bounds, data.words<4>("physics_footplantmanager", "default", "DeckBB"));
    s.deck_bounds = rs::from_bits(bounds);
    S3G_TRY(s.max_leg_angle_error, float_("MaxLegAngleError"));
    S3G_TRY(s.leg_length_on_landing, float_("LegLengthOnLanding"));
    S3G_TRY(s.foot_volume_y_offset, float_("FootVolumeYOffset"));
    S3G_TRY(s.deck_bounds_y_offset, float_("DeckBBYOffset"));
    for (std::size_t i = 0; i < 8; ++i) {
        s.radial_speed_scale.x[i] = graph[4 + i];
        s.radial_speed_scale.y[i] = graph[12 + i];
    }
    S3G_TRY(s.max_horizontal_speed, float_("Hash_149A5D0D4768D2B1"));
    S3G_TRY(s.max_descending_speed, float_("Hash_52436D9F4FE56BCB"));
    S3G_TRY(s.release_outward_speed, float_("Hash_C5F92168DFAFD0B8"));
    S3G_TRY(s.end_handle, float_("Hash_628C228DCC7B6ED5"));
    S3G_TRY(s.end_angle, float_("Hash_4F45DA86685CF029"));
    S3G_TRY(s.end_leg_length, float_("Hash_0DE0027B9913C5D5"));
    S3G_TRY(s.start_handle, float_("Hash_5C4341EF35F4B2B0"));
    S3G_TRY(s.min_duration, float_("Hash_D6224B710A36D643"));
    S3G_TRY(s.max_duration, float_("Hash_68C05A7FCC6BB718"));
    return s;
}

}  // namespace s3::game::physics::footplant::settings
