// Port of skate-game/src/physics/offboard_audit_trace.rs
//! TEMPORARY bottom-up audit observations; not native gameplay behavior.
//! Opt in with SKATE3_AUDIT_OFFBOARD_TICKS=start:end (inclusive simulation ticks).
//! Disabled by default. No gameplay state, guards, poses or forces are changed.
//
// C++ note: Rust prints several engine structs with `{:?}` (derived Debug). This port keeps
// the opt-in window semantics exactly and prints the scalar/array fields with Rust Debug
// formatting; composite structs without a C++ Debug impl are summarized (diagnostic only).
#include "s3/game/physics/offboard_audit_trace.hpp"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

#include "s3/core/physics/board.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/state_names.hpp"

namespace s3::game::physics::offboard_audit_trace {

namespace {

std::optional<std::pair<std::uint64_t, std::uint64_t>> parse_window() {
    const char* raw = std::getenv("SKATE3_AUDIT_OFFBOARD_TICKS");
    if (raw == nullptr) return std::nullopt;
    const std::string value(raw);
    const auto colon = value.find(':');
    if (colon == std::string::npos) S3_PANIC("Audit tick window must be start:end");
    auto parse = [](const std::string& text, const char* what) -> std::uint64_t {
        if (text.empty()) S3_PANIC(what);
        std::uint64_t v = 0;
        for (char c : text) {
            if (c < '0' || c > '9') S3_PANIC(what);
            const std::uint64_t next = v * 10u + std::uint64_t(c - '0');
            if (next / 10u != v) S3_PANIC(what);
            v = next;
        }
        return v;
    };
    const std::uint64_t start = parse(value.substr(0, colon), "Invalid audit start tick");
    const std::uint64_t end = parse(value.substr(colon + 1), "Invalid audit end tick");
    if (!(start <= end)) S3_PANIC("Audit tick window is reversed");
    return std::make_pair(start, end);
}

const std::optional<std::pair<std::uint64_t, std::uint64_t>>& window() {
    static const std::optional<std::pair<std::uint64_t, std::uint64_t>> w = parse_window();
    return w;
}

template <class C>
std::string floats(const C& c) {
    std::string s = "[";
    bool first = true;
    for (float v : c) {
        if (!first) s += ", ";
        s += fmt::debug_f32(v);
        first = false;
    }
    return s + "]";
}
template <class M>
std::string float_rows(const M& m) {
    std::string s = "[";
    bool first = true;
    for (const auto& row : m) {
        if (!first) s += ", ";
        s += floats(row);
        first = false;
    }
    return s + "]";
}
std::string hex8(std::uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%08x", v);
    return buf;
}
template <class C>
std::string hex_words(const C& c) {
    std::string s = "[";
    bool first = true;
    for (std::uint32_t v : c) {
        if (!first) s += ", ";
        s += hex8(v);
        first = false;
    }
    return s + "]";
}
template <class M>
std::string bit_rows(const M& m) {
    std::string s = "[";
    bool first = true;
    for (const auto& row : m) {
        if (!first) s += ", ";
        s += hex_words(rs::to_bits(row));
        first = false;
    }
    return s + "]";
}

}  // namespace

void stage(std::uint64_t tick, std::string_view phase, const GamePhysics& physics, const SkaterRuntime& s,
           const PlayerControls& controls) {
    const auto& w = window();
    if (!w) return;
    const auto [start, end] = *w;
    if (tick < start || tick > end) return;
    const auto& p = s.player_input.processed;
    const auto& roots = s.animated_skeleton.roots;
    const auto& deck = physics.board.bodies()[::s3::physics::board::index(::s3::physics::board::BodyId::Deck)];
    const std::string ph(phase);
    std::string reasons = "[";
    for (std::size_t i = 0; i < s.wipeout.state.reasons.size(); ++i) {
        if (i) reasons += ", ";
        reasons += s.wipeout.state.reasons[i] ? "true" : "false";
    }
    reasons += "]";
    log::eprintln(fmt::cat(
        "OFFBOARD_AUDIT_BAIL tick=", tick, " phase=", ph, " state=", debug_name(s.player_state.current()),
        " requests=Requests { reasons: ", reasons, ", values: ", floats(s.wipeout.state.values),
        ", count: ", s.wipeout.state.count, ", cooldown: ", fmt::debug_f32(s.wipeout.state.cooldown),
        ", contact_frames: ", s.wipeout.state.contact_frames, ", balance: ", fmt::debug_f32(s.wipeout.state.balance),
        ", mode: ", s.wipeout.state.mode, " } pose_error=", floats(s.collision_pose_error), " maximum_pose_error=",
        s.collision_maximum_error ? "Some(" + fmt::debug_f32(*s.collision_maximum_error) + ")" : std::string("None"),
        " root_velocity=", floats(s.skeleton_input.root_velocity)));
    log::eprintln(fmt::cat(
        "OFFBOARD_AUDIT tick=", tick, " phase=", ph, " state=", debug_name(s.player_state.current()),
        " controls_tick=", controls.ticks, " controller_words=", hex_words(controls.controller.words()),
        " flags=", hex_words(std::array<std::uint32_t, 5>{p.flags_2468, p.flags_2472, p.flags_2476, p.flags_2480,
                                                         p.flags_2484}),
        " root_bits=", bit_rows(roots.animation_to_world), " inverse_root_bits=", bit_rows(roots.world_to_animation),
        " physical_com=", floats(s.skeleton.record.centre_of_mass),
        " hand=", s.board_possession.state.selected_hand_424,
        " deck_position=", floats(std::array<float, 3>{deck.rates.position.x, deck.rates.position.y,
                                                       deck.rates.position.z}),
        " query_ready=", s.offboard_contact.readiness()));
    if (ph == "state" || ph == "solve") {
        std::string mapped = "[";
        for (std::size_t i = 0; i < s.animated_skeleton.record.pose.size(); ++i) {
            if (i) mapped += ", ";
            mapped += float_rows(s.animated_skeleton.record.pose[i]);
        }
        mapped += "]";
        std::string targets = "[";
        for (std::size_t i = 0; i < s.skeleton_input.drive_frames.size(); ++i) {
            if (i) targets += ", ";
            targets += float_rows(s.skeleton_input.drive_frames[i]);
        }
        targets += "]";
        std::string physical = "[";
        for (std::size_t i = 0; i < s.skeleton.record.pose.size(); ++i) {
            if (i) physical += ", ";
            physical += float_rows(s.skeleton.record.pose[i]);
        }
        physical += "]";
        log::eprintln(fmt::cat("OFFBOARD_AUDIT_POSE tick=", tick, " phase=", ph, " mapped=", mapped,
                               " targets=", targets, " physical=", physical));
    }
}

}  // namespace s3::game::physics::offboard_audit_trace
