// Port of skate-game/src/physics/offboard/settings/board.rs
#include "s3/game/physics/offboard/settings/board.hpp"

#include <array>
#include <cstdint>

namespace s3::game::physics::offboard::settings::board {

rs::Result<BoardSettings, std::string> load(const sd::Collections& data) {
    auto float_ = [&](const char* name) { return data.float_("physics_state_offboard", "default", name); };
    auto vector = [&](const char* name) -> rs::Result<std::array<float, 4>, std::string> {
        S3G_TRY(const sd::Field* field, data.field("physics_state_offboard", "default", name));
        if (field->type_name != "Math::Vector3") {
            return rs::make_err(fmt::cat(name, ": expected stock Vector3"));
        }
        S3G_TRY(auto words, data.words<4>("physics_state_offboard", "default", name));
        const std::array<float, 4> value = rs::from_bits(words);
        for (float v : value) {
            if (!rs::is_finite(v)) return rs::make_err(fmt::cat(name, ": non-finite vector"));
        }
        return value;
    };
    BoardSettings s{};
    S3G_TRY(s.extent_0, vector("GrabBoxSizeGrabbing"));
    S3G_TRY(s.extent_16, vector("GrabBoxSize"));
    S3G_TRY(s.offset_32, vector("GrabBoxOffset"));
    S3G_TRY(s.angle_436, float_("GrabSplineMaxAngleToHorizontalGrabbing"));
    S3G_TRY(s.angle_440, float_("GrabSplineMaxAngleToHorizontal"));
    S3G_TRY(s.margin_444, float_("GrabSplineEndExclusion"));
    S3G_TRY(s.angle_452, float_("GrabSplineAngleLimitGrabbing"));
    S3G_TRY(s.angle_456, float_("GrabSplineAngleLimit"));
    return s;
}

}  // namespace s3::game::physics::offboard::settings::board
