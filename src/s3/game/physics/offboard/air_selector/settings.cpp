// Port of skate-game/src/physics/offboard/air_selector/settings.rs
#include "s3/game/physics/offboard/air_selector/settings.hpp"

#include <cstdint>

namespace s3::game::physics::offboard::air_selector::settings {

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data) {
    const char* class_ = "physics_state_offboard_air";
    sc::player::offboard::air_selector::Settings query{};
    S3G_TRY(query.height, data.float_(class_, "default", "Hash_AB0D9EAEBFC584E9"));
    S3G_TRY(query.sphere_radius, data.float_(class_, "default", "TrajectorySphereRadius"));
    S3G_TRY(std::uint32_t start_index, data.integer(class_, "default", "TrajectoryStartIndex"));
    query.start_index = static_cast<std::int32_t>(start_index);
    if (!rs::is_finite(query.height) || !rs::is_finite(query.sphere_radius) || query.sphere_radius <= 0.0f) {
        return rs::make_err(std::string("Invalid stock BipedAir height/radius"));
    }
    S3G_TRY(auto blend, curves::load<8>(data, class_, "TrajBlendAmountVsTime"));
    S3G_TRY(float deck_center_to_truck, data.float_("physics_grinds", "default", "DeckCenterToTruck"));
    if (!rs::is_finite(deck_center_to_truck) || deck_center_to_truck <= 0.0f) {
        return rs::make_err(std::string("Invalid stock DeckCenterToTruck"));
    }
    return Settings{query, blend.first, deck_center_to_truck};
}

}  // namespace s3::game::physics::offboard::air_selector::settings
