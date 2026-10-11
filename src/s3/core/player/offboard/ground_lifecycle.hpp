// Port of skate-core/src/player/offboard/ground_lifecycle.rs (+ collision.rs, exit.rs,
// post.rs, publication.rs).
// BipedGround Exit82D30CC0, PostPhysics82D32AA0 and FillPhysOut82D32D38.
// Borrows canonical Ground state; source writes only the returned packet fields.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/offboard/ground_entry.hpp"
#include "s3/core/player/wipeout.hpp"

namespace s3::player::offboard::ground_lifecycle {

using ground_entry::Vector;

namespace detail {
inline float dot(Vector a, Vector b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
float length(Vector v);
}  // namespace detail

// ---- collision.rs: complete82D90148, borrowing the existing shared Wipeout request owner.
struct CollisionSettings {
    ///physics_wipeout/default layout96..120.
    float vehicle_scalar = 0.0f;
    float vehicle_contact = 0.0f;
    float maximum_displacement = 0.0f;
    float maximum_arm_contact = 0.0f;
    float maximum_body_contact = 0.0f;
    float minimum_speed = 0.0f;
    float maximum_squash = 0.0f;
    ///Actual attribute472174920C68FBE3. Required when processed2480bit7.
    float special_scalar = 0.0f;
};

struct CollisionInput {
    const wipeout::Frame* shared = nullptr;
    Vector skeleton_velocity_16336{};
    bool contact_flag_4072 = false;
    float contact_force_4056 = 0.0f;
};

void check_collision(wipeout::Requests& requests, const CollisionSettings& s, const CollisionInput& i);

// ---- exit.rs: 82D30CC0 lifecycle ordering. Toolkit refresh is a real prerequisite service.
struct ExitServices {
    virtual ~ExitServices() = default;
    ///Run complete original82D81610 against the actual toolkit/contact owner.
    ///Its active-contact branch must not be replaced by a reset/no-op.
    virtual void refresh_toolkit_contacts() = 0;
    ///After refresh, zero actual toolkit320 and30452/30456/30460/30464/30468.
    virtual void clear_toolkit_exit_fields() = 0;
    virtual bool ground_geometry_active() const = 0;
    ///Actual Groundstate76 geometry service virtual12 release.
    virtual void release_ground_geometry() = 0;
    virtual void clear_ground_geometry_active() = 0;
};

inline void exit(ExitServices& services) {
    services.refresh_toolkit_contacts();
    services.clear_toolkit_exit_fields();
    if (services.ground_geometry_active()) {
        services.release_ground_geometry();
        services.clear_ground_geometry_active();
    }
}

// ---- post.rs
struct PostInput {
    CollisionInput collision{};
    std::uint32_t processed_flags_2484 = 0;
    Vector processed_velocity_608{};
    Vector skeleton_displacement_16288{};
    Vector skeleton_displacement_16304{};
    std::uint32_t ground_kind_356 = 0;
};

void post_physics(ground_entry::State& state, wipeout::Requests& requests, const CollisionSettings& settings,
                  const PostInput& input);

// ---- publication.rs
struct PublicationInput {
    std::array<bool, 3> ground_flags_752_to_754{};
    std::uint32_t contact_flags_368 = 0;
    Vector contact_position_192{};
    Vector query_position_816{};
    std::uint32_t processed_flags_2476 = 0;
    std::uint32_t ground_kind_356 = 0;
    float ground_scalar_360 = 0.0f;
    Vector motion_vector_1040{};
    Vector motion_up_864{};
    ///Actual hand manager flags105/106/107 and217/218/219.
    std::array<std::array<bool, 3>, 2> hand_flags{};
};

///Only fields written by82D32D38/82D785F8; caller preserves all other output.
struct Publication {
    std::uint32_t physics_counter_36 = 0;
    std::uint32_t physics_counter_40 = 0;
    bool physics_flag_86 = false;
    bool offboard_flag_304 = false;
    std::uint32_t offboard_kind_88 = 0;
    float offboard_scalar_112 = 0.0f;
    bool offboard_flag_329 = false;
    bool offboard_flag_330 = false;
    float offboard_distance_116 = 0.0f;
    bool offboard_flag_334 = false;
    float offboard_scalar_32 = 0.0f;
    bool offboard_flag_328 = false;
    Vector animation_vector_144{};
    bool animation_flag_164 = false;
    std::array<bool, 2> offboard_hand_flags_306_307{};
};
S3_DERIVE_EQ(Publication, physics_counter_36, physics_counter_40, physics_flag_86, offboard_flag_304,
             offboard_kind_88, offboard_scalar_112, offboard_flag_329, offboard_flag_330, offboard_distance_116,
             offboard_flag_334, offboard_scalar_32, offboard_flag_328, animation_vector_144, animation_flag_164,
             offboard_hand_flags_306_307)

Publication publish(const ground_entry::State& state, const PublicationInput& i);

}  // namespace s3::player::offboard::ground_lifecycle
