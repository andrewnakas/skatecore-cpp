// Port of skate-core/src/player/respawn.rs (whole file).
// TU3 actor checkpoint history (82BFB068..82BFCB38).
// Scene predicates are supplied by the host; absence is never acceptance.
//
// Rust `trait Validation { type Error; ... }` -> duck-typed template parameter V
// with `using Error = ...;` and methods ground/location/occupants/edges returning
// rs::Result<std::optional<Ground>, Error> / rs::Result<bool, Error>.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::respawn {

using Matrix = physics::skeleton_animation_record::AnimationPartTransform;
using physics::skeleton_animation_record::IDENTITY;

struct Candidate {
    Matrix transform{};
    std::uint32_t stance = 0;
    bool offboard = false;
    float score = 0.0f;
};
S3_DERIVE_EQ(Candidate, transform, stance, offboard, score)

struct Ground {
    std::array<float, 4> position{};
    bool offboard = false;
};

///Completed native packet fields. The caller must provide actual producer
///values, especially Ground323 and conditioner164; neither means grounded.
struct Observation {
    std::int32_t measurements = 0;
    std::array<float, 4> root_position{};  // Skeleton416, unmirrored11920 translation
    std::array<float, 4> com_position{};   // Skeleton64, physical COM16144
    bool teleport_requested = false;       // State69
    std::uint32_t physical_state = 0;      // State16
    std::uint32_t state_frames = 0;        // State4
    bool ground_suppressed = false;        // Ground323
    bool offboard_correction = false;      // OffBoard333
    std::uint32_t ground_category = 0;     // Collision16 low16
    std::array<std::uint32_t, 2> foot_categories{};  // OffBoard52/56 packed categories
    Matrix riding_transform{};             // 82591E30(false)
    Matrix offboard_transform{};           // 82591E30(true)
    std::uint32_t stance = 0;              // 82592B68
    bool alternate_world = false;          // SimController virtual0
};

///82BFB3F8 rejects these categories; category8 explicitly requests offboard.
inline bool surface_allowed(std::uint32_t category) {
    switch (category) {
        case 5: case 6: case 9: case 12: case 13: return false;
        default: return true;
    }
}

///Jump table82BFB698: category1/2=100,3/4=50,11=20, all others=0.
inline float surface_score(std::uint32_t category) {
    switch (category) {
        case 1: case 2: return 100.0f;
        case 3: case 4: return 50.0f;
        case 11: return 20.0f;
        default: return 0.0f;
    }
}

namespace detail {
inline float distance_squared(std::array<float, 4> a, std::array<float, 4> b) {
    return rs::powi(a[0] - b[0], 2) + rs::powi(a[1] - b[1], 2) + rs::powi(a[2] - b[2], 2);
}
}  // namespace detail

class History {
public:
    // Private in Rust (`initial`, `entries`, `cooldown`); public here for the ported tests.
    Candidate initial;
    std::vector<Candidate> entries;  // native33 slots reserve one sentinel:32 entries
    std::int32_t cooldown = 20;
    Matrix current_orientation = IDENTITY;  // 208; only eligible riding states write it
    std::array<float, 4> current_position{};  // 272; every actor update writes Skeleton64

    static History new_(Candidate initial) {
        History h;
        h.initial = initial;
        h.entries = {initial};
        h.cooldown = 20;
        h.current_orientation = IDENTITY;
        h.current_position = initial.transform[3];
        return h;
    }

    ///82BFB1E0/82BFB3F8. Do not call this with last-grounded observations;
    ///the native actor consumes one completed physical publication per update.
    template <class V>
    rs::Result<rs::Unit, typename V::Error> observe(const Observation& input, std::uint32_t minimum_state_frames,
                                                    V& scene) {
        using R = rs::Result<rs::Unit, typename V::Error>;
        current_position = input.com_position;
        if (!recording_due(input.measurements, input.root_position) || input.teleport_requested) {
            return R(rs::Unit{});
        }
        Matrix transform;
        bool offboard;
        float score;
        switch (input.physical_state) {
            case 100:
            case 104:
                // This write precedes both the15-frame gate and terrain checks.
                current_orientation = input.riding_transform;
                if (input.state_frames <= minimum_state_frames || input.ground_suppressed ||
                    !surface_allowed(input.ground_category)) {
                    return R(rs::Unit{});
                }
                transform = input.riding_transform;
                offboard = input.ground_category == 8;
                score = surface_score(input.ground_category);
                break;
            case 500:
            case 502:
                if (input.state_frames <= minimum_state_frames || input.offboard_correction) {
                    return R(rs::Unit{});
                }
                transform = input.offboard_transform;
                offboard = true;
                score = rs::max(surface_score(input.foot_categories[0]), surface_score(input.foot_categories[1]));
                break;
            default:
                return R(rs::Unit{});
        }
        {
            auto location = scene.location(transform);
            if (location.is_err()) return R(rs::make_err(location.unwrap_err()));
            if (!location.unwrap()) return R(rs::Unit{});
            auto edges = scene.edges(transform);
            if (edges.is_err()) return R(rs::make_err(edges.unwrap_err()));
            if (!edges.unwrap()) return R(rs::Unit{});
        }
        if (!input.alternate_world) {
            auto ground = scene.ground(transform);
            if (ground.is_err()) return R(rs::make_err(ground.unwrap_err()));
            if (!ground.unwrap().has_value()) return R(rs::Unit{});
        }
        // The recording path discards the ray's offboard byte and hit position.
        insert(Candidate{transform, input.stance, offboard, score});
        return R(rs::Unit{});
    }

    ///82BFB1E0: call even when the physical state cannot supply a candidate.
    ///The120 gate uses conditioner164 (EmpiricalMeasurements82DE5858), not
    ///State4's time in the current state. Its reset lifecycle is caller-owned.
    bool recording_due(std::int32_t measurements, std::array<float, 4> root_position) {
        bool eligible = measurements >= 120;
        if (!eligible) {
            cooldown = 0;
        }
        if (cooldown > 0) {
            cooldown -= 1;
            eligible = false;
        }
        if (!eligible) return false;
        for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            if (detail::distance_squared(it->transform[3], root_position) < 2.25f) return false;
        }
        return true;
    }

    ///82BFCB38. Construction and successful recording both refresh cooldown.
    void insert(Candidate candidate) {
        if (entries.size() == 32) {
            entries.erase(entries.begin());
        }
        entries.push_back(candidate);
        cooldown = 20;
    }

    ///82BFC9B0, used by automatic actor reset82592518. Manual/session-marker
    ///selection82BFC828 has a different history walk and must not call this.
    template <class V>
    rs::Result<Candidate, typename V::Error> automatic(std::uint32_t stance, V& scene) {
        using R = rs::Result<Candidate, typename V::Error>;
        Matrix transform = current_orientation;
        transform[3] = current_position;
        auto ground = scene.ground(transform);
        if (ground.is_err()) return R(rs::make_err(ground.unwrap_err()));
        if (ground.unwrap().has_value()) {
            const Ground g = *ground.unwrap();
            auto location = scene.location(transform);
            if (location.is_err()) return R(rs::make_err(location.unwrap_err()));
            bool ok = location.unwrap();
            if (ok) {
                auto occupants = scene.occupants(transform);
                if (occupants.is_err()) return R(rs::make_err(occupants.unwrap_err()));
                ok = occupants.unwrap();
            }
            if (ok) {
                auto edges = scene.edges(transform);
                if (edges.is_err()) return R(rs::make_err(edges.unwrap_err()));
                ok = edges.unwrap();
            }
            if (ok) {
                if (g.offboard) {
                    transform[3][1] = g.position[1];
                }
                return R(Candidate{transform, stance, g.offboard, 0.0f});
            }
        }
        while (auto candidate = pop_best()) {
            auto location = scene.location(candidate->transform);
            if (location.is_err()) return R(rs::make_err(location.unwrap_err()));
            if (!location.unwrap()) continue;
            auto occupants = scene.occupants(candidate->transform);
            if (occupants.is_err()) return R(rs::make_err(occupants.unwrap_err()));
            if (occupants.unwrap()) return R(*candidate);
        }
        // 82BFC718 also serves non-destructive selectors; retain its actual rule.
        if (!entries.empty()) return R(entries.back());
        return R(Candidate{initial.transform, stance, false, 0.0f});
    }

private:
    History() = default;

    ///82BFC550: descending score, newest wins ties; selected entry is removed
    ///before validation, including successful selections. Repeated bails thus
    ///cannot keep selecting the same history entry.
    std::optional<Candidate> pop_best() {
        std::optional<std::size_t> best;
        float score = -99990.0f;
        std::size_t age = 0;
        for (std::size_t k = entries.size(); k-- > 0; ++age) {
            const std::size_t index = k;
            float value = entries[index].score;
            if (age < 1) {
                value -= 100.0f;
            }
            if (age > 5) {
                value -= 200.0f;
            }
            if (value > score) {
                best = index;
                score = value;
            }
        }
        if (!best) return std::nullopt;
        const Candidate c = entries[*best];
        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(*best));
        return c;
    }
};

}  // namespace s3::player::respawn
