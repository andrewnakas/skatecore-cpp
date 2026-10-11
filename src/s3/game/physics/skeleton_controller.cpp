// Port of skate-game/src/physics/skeleton_controller.rs.
#include "s3/game/physics/skeleton_controller.hpp"

namespace s3::game::physics::skeleton_controller {

Status SkeletonControllerState::request_ground(sc::physics::skeleton_body::SkeletonCollisionMode& collision) {
    return request(6, collision);
}

Status SkeletonControllerState::request(std::uint32_t requested_,
                                        sc::physics::skeleton_body::SkeletonCollisionMode& collision) {
    if (auto mode = select_request(requested_)) {
        auto result = collision.select_driven(*mode);
        if (result.is_err()) return rs::make_err(std::string(result.unwrap_err()));
    }
    return ok();
}

std::optional<std::uint32_t> SkeletonControllerState::select_request(std::uint32_t requested_) {
    if (has_request && requested == requested_) return std::nullopt;
    std::uint32_t effective_;
    if (override_enabled) {
        requested = requested_;
        has_request = true;
        //82D911D8..1264: unsigned requested-4 indexes the eight cases.
        if (requested_ == 4) {
            effective_ = 1;
        } else if (requested_ >= 5 && requested_ <= 7) {
            effective_ = 2;
        } else if (requested_ == 8) {
            effective_ = 3;
        } else if (requested_ >= 9 && requested_ <= 11) {
            effective_ = requested_;
        } else {
            return std::nullopt;
        }
    } else {
        if (effective >= 1 && effective <= 2) return std::nullopt;
        effective_ = requested_;
    }
    effective = effective_;
    const std::uint32_t mode = effective_ - 1u;  // wrapping_sub
    if (mode > 10) return std::nullopt;
    return mode;
}

Status SkeletonControllerState::update_air(std::uint32_t processed_flags_2472, float velocity_y,
                                           sc::physics::skeleton_body::SkeletonCollisionMode& collision) {
    const bool ascending = (processed_flags_2472 & 0x8000u) != 0 || velocity_y > 0.0f;
    if (flag_18 != ascending) {
        flag_18 = ascending;
        S3G_CHECK(request(ascending ? 7 : 6, collision));
    }
    return ok();
}

}  // namespace s3::game::physics::skeleton_controller
