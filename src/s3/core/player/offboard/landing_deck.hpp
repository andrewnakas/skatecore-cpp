// Port of skate-core/src/player/offboard/landing_deck.rs (+ query.rs, trajectory.rs,
// types.rs, update.rs).
// Shared LandingOnDeckManager68 for BipedAir501 and LandingOnDeck503.
// Original S3 TU3 SHA256:
// 431b8eba23565affdc10d137df19b06fe286244cefb3e1a13f32693e9600395a.
// Host submits returned queries and acknowledges success; no world/solver owner.
#pragma once
#include <optional>

#include "s3/core/player/_deps/air_trajectory.hpp"
#include "s3/core/player/offboard/landing_deck/trajectory.hpp"
#include "s3/core/player/offboard/landing_deck/types.hpp"
#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::offboard::landing_deck {

using air::trajectory::QueryResult;
using air::trajectory::Trajectory;

inline const float STEP = rs::from_bits(0x3c888889u);
inline const Vector GRAVITY{0.0f, rs::from_bits(0xc11ccccdu), 0.0f, 0.0f};
inline const Trajectory EMPTY{{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, -1.0f};

///One retained owner. Public fields permit the source-backed503 InitTraj producer
///to initialize this SAME trajectory and force flag, not a parallel state object.
struct Manager {
    ///Default: host allocation seed, NOT a substitute for503 InitTraj or Air assistance.
    Trajectory trajectory_32 = EMPTY;
    Trajectory proposed_96 = EMPTY;
    float elapsed_160 = 0.0f;
    bool trajectory_valid_164 = false;
    Vector ik_offset_176{};
    Vector vector_192{};
    Vector moving_contact_208{};
    Vector vector_224{};
    float obstruction_height_240 = 0.0f;
    float time_to_land_244 = 0.0f;
    float proposed_time_248 = 0.0f;
    std::uint32_t completed_queries_252 = 0;
    bool can_land_256 = false;
    bool force_257 = false;
    bool blocked_258 = false;
    bool tested_259 = false;
    bool hippy_hurdling_260 = false;
    bool publish_moving_contact_261 = false;
    bool pending_262 = false;

    ///82D78B28: notably does NOT clear proposed96 or outstanding query262.
    void reset();
    ///82D78C38: preserve the native correction sign and positive-frame guard.
    void correct_trajectory(Vector com);
    ///82D79420. None means no write to skeleton3482/48, not clear those fields.
    FillOutput fill() const;

    // ---- query.rs ----
    ///82D79948 numeric request (pub(super)).
    QueryRequest prepare_query(const Input& p, Vector position, float time);
    ///Call only after the returned request has actually been submitted.
    void query_submitted() { pending_262 = true; }
    ///Consume one COMPLETED real query. None (or an explicit invalid result)
    ///means no collision. `hippy_velocity(const Manager&) -> rs::Result<Vector, E>`.
    ///Its error leaves manager state unchanged so the host can propagate/retry.
    template <class E, class F>
    rs::Result<rs::Unit, E> sync(std::optional<QueryResult> hit, const SyncInput& input, F&& hippy_velocity) {
        Manager next = *this;
        if (hit && !hit->valid()) hit.reset();
        auto r = next.sync_completed<E>(hit, input, std::forward<F>(hippy_velocity));
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        *this = next;
        return rs::Unit{};
    }

    // ---- trajectory.rs ----
    ///82D78EE8. Failed intersection clears only can-land; other fields retain.
    void consider_board(const Input& p, const Settings& settings, Vector position, Vector velocity);

    // ---- update.rs ----
    ///82D78D30.
    std::optional<QueryRequest> assist(const AssistInput& input, const Settings& settings);
    ///82D794A0.
    UpdateOutput update(const Input& p, const Settings& settings);
    ///82D792D0.
    Vector calculate_accurate_ik_offset(const IkInput& input);

private:
    template <class E, class F>
    rs::Result<rs::Unit, E> sync_completed(std::optional<QueryResult> hit, const SyncInput& input,
                                           F&& hippy_velocity);
};
S3_DERIVE_EQ(Manager, trajectory_32, proposed_96, elapsed_160, trajectory_valid_164, ik_offset_176, vector_192,
             moving_contact_208, vector_224, obstruction_height_240, time_to_land_244, proposed_time_248,
             completed_queries_252, can_land_256, force_257, blocked_258, tested_259, hippy_hurdling_260,
             publish_moving_contact_261, pending_262)

template <class E, class F>
rs::Result<rs::Unit, E> Manager::sync_completed(std::optional<QueryResult> hit, const SyncInput& input,
                                                F&& hippy_velocity) {
    namespace m = wipeout_state::math;
    using trajectory::frames;
    using trajectory::shift;
    if (hit) {
        // 82D79B9C uses strict less-than: equality is an obstruction.
        if (hit->contact_position[1] < obstruction_height_240) {
            blocked_258 = false;
            tested_259 = true;
        } else if (tested_259) {
            blocked_258 = true;
        } else if (completed_queries_252 == 0) {
            moving_contact_208 = hit->contact_transform[3];
            publish_moving_contact_261 = true;
        } else {
            const Vector contact = hit->contact_transform[3];  // native result+112, NOT normal
            const Vector velocity = m::scale(m::sub(contact, moving_contact_208), 60.0f);
            if (m::dot(velocity, velocity) > rs::from_bits(0x3efae147u)) {
                hippy_hurdling_260 = true;
                if (completed_queries_252 == 1) {
                    if ((input.flags_2488 & 0x04000000u) != 0) {
                        // 79E30 compares full-precision elapsed velocity BEFORE
                        // quantizing the epoch shift to integer frames.
                        const Vector current = trajectory_32.velocity_at(elapsed_160);
                        rs::Result<Vector, E> replacement_r = hippy_velocity(static_cast<const Manager&>(*this));
                        if (replacement_r.is_err()) return rs::make_err(replacement_r.unwrap_err());
                        const Vector replacement = replacement_r.unwrap();
                        if (replacement[1] > current[1]) {
                            const std::int32_t frame = frames(elapsed_160);
                            shift(trajectory_32, frame);
                            trajectory_32.velocity = replacement;
                            shift(trajectory_32, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(frame)));
                        }
                    }
                } else {
                    const float landing_time = time_to_land_244 + elapsed_160;
                    const Vector predicted_contact = m::madd(velocity, time_to_land_244, contact);
                    Vector separation = m::sub(trajectory_32.position_at(landing_time), predicted_contact);
                    separation[1] = 0.0f;
                    const float square = m::dot(separation, separation);
                    if (trajectory_32.velocity_at(elapsed_160)[1] < 0.0f) {
                        if (input.position_592[1] - hit->contact_position[1] < 0.7f) {
                            blocked_258 = true;
                            tested_259 = true;
                        }
                    } else if (square < 1.0f) {
                        blocked_258 = true;
                        tested_259 = true;
                    } else if (square > 6.25f) {
                        // Big-endian decompiled sth1 at+258 is bytes00,01.
                        // Saved disassembly79DDC/DE0 proves these two stores.
                        blocked_258 = false;
                        tested_259 = true;
                    }
                    // Intermediate separation [1,6.25] retains both flags.
                }
            } else {
                hippy_hurdling_260 = false;
                blocked_258 = true;
                tested_259 = true;
            }
            moving_contact_208 = contact;
        }
    } else {
        blocked_258 = false;
        tested_259 = true;
    }
    pending_262 = false;
    completed_queries_252 = completed_queries_252 + 1u;
    return rs::Unit{};
}

}  // namespace s3::player::offboard::landing_deck
