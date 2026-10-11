// Port of skate-core/src/player/offboard/ground_sync.rs (whole file).
// TU3 BipedGround Sync82D31DC0, air decision82D32338 and board update82D324B0.
// Call only after the Biped job completes. Geometry submitted here is consumed
// by the next PreUpdate; this function never fabricates collision observations.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/offboard/ground_entry.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/core/player/offboard/ground_sync/math.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::ground_sync {

using ground_entry::Frame;
using ground_entry::State;
using ground_entry::Vector;

struct CompletedMotion {
    Vector contact_position_192{};
    std::uint32_t contact_flags_368 = 0;
    Frame physical_frame_848{};
    Frame animation_frame_912{};
    Vector vector_976{};
    Vector vector_992{};
    Vector vector_1008{};
    Vector vector_1040{};
    Vector vector_1056{};
    bool launch_1076 = false;
    bool flag_1077 = false;
};
struct Processed {
    Frame frame_192{};
    Vector position_592{};
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t flags_2488 = 0;
    float elapsed_2664 = 0.0f;
    float value_2852 = 0.0f;
    ground_query::QueryContext query_context{};
};
/// Actual state40 settings. Values are loaded attributes, with no inferred defaults.
struct BoardSettings {
    Vector extent_0{};
    Vector extent_16{};
    Vector offset_32{};
    float angle_436 = 0.0f;
    float angle_440 = 0.0f;
    float margin_444 = 0.0f;
    float angle_452 = 0.0f;
    float angle_456 = 0.0f;
};
struct Bounds {
    Frame frame{};
    Vector extents{};
};
struct BoardLimits {
    float margin = 0.0f;
    float angle_a = 0.0f;
    float angle_b = 0.0f;
};
/// 82D2DCC0's seven vectors are all overwritten by Sync before82D811C8.
struct ToolkitInput {
    std::array<Vector, 7> _0{};
};

/// Canonical physical owners implement these operations against live state.
/// (Rust trait `Services: GroundQueryScene` with associated Launch/Candidate.
/// `update_board`/`sync` are templates accepting any type with these members;
/// this abstract base is a convenience.)
template <class E, class LaunchT, class CandidateT>
struct Services : ground_query::GroundQueryScene<E> {
    using Launch = LaunchT;
    using Candidate = CandidateT;
    virtual Processed processed() const = 0;
    /// 82D2DB98 initialization then82D7BA78 with final argument false.
    virtual Launch prepare_biped_launch() = 0;
    virtual Vector skeleton_point_10960() const = 0;
    /// Store point at launch32, then execute82D6CA58 against BipedAir.
    virtual void launch_air(Launch launch, Vector point) = 0;
    virtual void update_skeleton(Frame frame, Vector vector_1056) = 0;
    virtual BoardSettings board_settings() const = 0;
    virtual std::uint8_t board_flags_12836() const = 0;
    /// 82D2CF68 initialized candidate,82D4D150 mode0 and initially zero key.
    virtual std::optional<Candidate> probe_board(Vector position) = 0;
    virtual std::array<std::uint32_t, 2> candidate_key_188(const Candidate& candidate) const = 0;
    /// Actual82BE3220 bone23 transform's position48.
    virtual Vector bone_23_position() = 0;
    /// 82E08DB8, including candidate projection and82E08EE8 geometry tests.
    virtual bool classify_board(const Candidate& candidate, Vector bone, Bounds bounds, BoardLimits limits) = 0;
    /// 82D74200 mode4. This runs even when probe/classification fails.
    virtual void query_board(Vector position, Bounds bounds, BoardLimits limits) = 0;
    virtual void bind_board(std::array<std::uint32_t, 2> key) = 0;
    /// Board action virtual44, with live manager12828 after binding.
    virtual void commit_bound_board() = 0;
    /// Board action virtual24 with original processed frame and live manager fields.
    virtual void commit_free_board(Frame frame) = 0;
    /// 82D749D0 followed by clearing manager12780.
    virtual void reset_board() = 0;
    virtual void update_toolkit(ToolkitInput input) = 0;
    virtual float deck_half_wheelbase() const = 0;
    /// Submit all seven real collision lines; retain until next PreUpdate consumes.
    virtual void submit_ground_query(ground_query::GroundQueryPacket packet) = 0;
};

/// 82D32338: special contact/input flags suppress only the explicit jump request.
/// Loss of contact and the controller's launch byte still trigger an air launch.
bool wants_air(const State& state, const CompletedMotion& motion, const Processed& p);
/// 82D2E250 with argument7=false: retain forward.w while flattening only y.
Bounds board_bounds(const Frame& frame, Vector offset, Vector extents);

namespace detail {
inline BoardLimits limits(float margin, float a, float b) {
    const float radians = rs::from_bits(0x3c8efa35u);
    return BoardLimits{margin, a * radians, b * radians};
}
inline s3::math::Vector3 xyz(Vector v) { return s3::math::Vector3::new_(v[0], v[1], v[2]); }
inline ground_query::Frame query_frame(const Frame& f) {
    return ground_query::Frame{xyz(f[0]), xyz(f[1]), xyz(f[2]), xyz(f[3])};
}
}  // namespace detail

/// 82D324B0. The caller clears144; a failed probe deliberately preserves that byte.
template <class S>
void update_board(State& state, S& services) {
    using namespace ground_sync::math;
    const BoardSettings settings = services.board_settings();
    const Frame original_frame = services.processed().frame_192;
    const Bounds bounds = board_bounds(original_frame, settings.offset_32, settings.extent_16);
    const Vector position = services.processed().position_592;
    if ((services.board_flags_12836() & 0x40) != 0) {
        if (auto candidate = services.probe_board(position)) {
            // The probe may change live processed/settings state.
            const BoardSettings s1 = services.board_settings();
            const Frame frame = services.processed().frame_192;
            const Bounds inner = board_bounds(frame, s1.offset_32, scale(s1.extent_0, rs::from_bits(0x3f733333u)));
            const BoardSettings s2 = services.board_settings();
            const BoardLimits lim = detail::limits(s2.margin_444, s2.angle_452, s2.angle_436);
            const Vector bone = services.bone_23_position();
            state.flags_144_to_150[0] = services.classify_board(*candidate, bone, inner, lim);
            if (state.flags_144_to_150[0]) {
                const auto key = services.candidate_key_188(*candidate);
                state.counter_152 = key[0];
                state.counter_156 = key[1];
            }
        }
    }
    const BoardSettings s3_ = services.board_settings();
    services.query_board(position, bounds, detail::limits(s3_.margin_444, s3_.angle_456, s3_.angle_440));
    if (state.flags_144_to_150[0]) {
        services.bind_board({state.counter_152, state.counter_156});
        services.commit_bound_board();
    } else {
        services.commit_free_board(original_frame);
    }
}

/// Full Sync order; returns whether a new geometry packet was submitted.
/// Scene failure is propagated after the already completed physical updates.
template <class S>
rs::Result<bool, typename S::Error> sync(State& state, const CompletedMotion& motion, S& services) {
    using namespace ground_sync::math;
    state.flags_144_to_150[2] = wants_air(state, motion, services.processed());
    if (state.flags_144_to_150[2]) {
        auto launch = services.prepare_biped_launch();
        Vector point = services.skeleton_point_10960();
        const Frame frame = motion.animation_frame_912;
        point = madd(frame[2], point[2], madd(frame[1], point[1], madd(frame[0], point[0], frame[3])));
        services.launch_air(std::move(launch), point);
    }
    state.flags_144_to_150[0] = false;
    state.flags_144_to_150[3] = motion.flag_1077;
    state.frame_80 = motion.physical_frame_848;
    if ((motion.contact_flags_368 & 1u) != 0) {
        const Vector up = state.frame_80[1];
        const float distance = dot(sub(motion.contact_position_192, state.frame_80[3]), up);
        if (!(distance <= 0.0f)) state.frame_80[3] = madd(up, distance, state.frame_80[3]);
    }
    Frame animation = motion.animation_frame_912;
    if (state.flags_144_to_150[6]) {
        state.duration_180 -= rs::from_bits(0x3c888889u);
        if (!(state.duration_180 > 0.0f)) {
            state.duration_180 = 0.0f;
            state.flags_144_to_150[6] = false;
        }
        state.angle_172 *= rs::from_bits(0x3f666666u);
        const float angle = rs::mul_add(-state.angular_velocity_176, state.duration_180, state.angle_172);
        const auto [sin, cos] = trigonometry::sin_cos(angle);
        const Vector forward =
            madd(Vector{sin, 0.0f, cos, sin}, animation[2][2],
                 madd(Vector{0.0f, 1.0f, 0.0f, 0.0f}, animation[2][1], scale(Vector{cos, 0.0f, -sin, cos}, animation[2][0])));
        // Native branch has no zero fallback on either cross normalization.
        const Vector right = normalize_unchecked(cross(animation[1], forward));
        animation[0] = right;
        animation[1] = normalize_unchecked(cross(forward, right));
        animation[2] = forward;
    }
    services.update_skeleton(animation, motion.vector_1056);
    Processed p = services.processed();
    if ((p.flags_2484 & 0x20000000u) == 0 && (p.flags_2480 & 0x00040000u) == 0 && !(p.value_2852 > 0.0f)) {
        if ((p.flags_2476 & 0x00400000u) != 0) {
            if (!state.flags_144_to_150[4]) {
                update_board(state, services);
            } else if (!(p.elapsed_2664 <= 0.5f)) {
                state.flags_144_to_150[4] = false;
            }
        } else {
            services.reset_board();
        }
    }
    services.update_toolkit(ToolkitInput{{state.frame_80[3], motion.vector_1008, motion.vector_992, motion.vector_976,
                                          motion.vector_1040, animation[1], animation[0]}});
    p = services.processed();
    const ground_query::Frame frame = detail::query_frame(state.frame_80);
    const auto search = ground_query::edge_search(frame, p.query_context, detail::xyz(motion.vector_1040), p.flags_2488);
    auto candidates = services.edge_candidates(search);
    if (candidates.is_err()) return rs::make_err(candidates.unwrap_err());
    if (auto edge = ground_query::select_edge(search, candidates.unwrap())) {
        if (auto packet = ground_query::prepare_packet(frame, p.query_context, *edge, services.deck_half_wheelbase())) {
            services.submit_ground_query(*packet);
            return true;
        }
    }
    return false;
}

}  // namespace s3::player::offboard::ground_sync
