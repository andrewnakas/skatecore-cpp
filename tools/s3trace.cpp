// s3trace <core.pack> <map.skate|test-world> <input script> <ticks> [--difficulty easy|normal|hardcore] [--hex]
//
// C++ port of simref crates/sim/src/bin/trace.rs: loads the cleanroom pack (mounted at "", asset
// root "assets") and a .skate map, runs one fixed tick per host frame exactly like simref's
// driver.rs (Sim::load / Sim::step), and prints the same CSV (stdout; logs on stderr).
// Script lines: <from_tick> <to_tick> lx ly rx ry buttons [lt rt]   (inclusive range,
//   sticks -1..1 with +y = up, buttons = XInput wButtons bitmask in decimal or 0x hex,
//   triggers 0..1). '#' starts a comment. Later lines win.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "s3/core/rs.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"
#include "s3/game/driver.hpp"

using s3::game::driver::Pad;
using s3::game::driver::Sim;
namespace rs = s3::rs;

namespace {

std::int16_t axis(float v) {
    // Same conversion the game's web input path uses to synthesize the XInput packet.
    return rs::as_int<std::int16_t>(rs::clamp(v, -1.0f, 1.0f) * 32767.0f);
}
std::uint8_t trigger(float v) { return rs::as_int<std::uint8_t>(rs::clamp(v, 0.0f, 1.0f) * 255.0f); }

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n\v\f";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

[[noreturn]] void die(const std::string& msg, int code) {
    std::fprintf(stderr, "%s\n", msg.c_str());
    std::exit(code);
}

float parse_f32(const std::string& s, std::size_t line) {
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (s.empty() || end != s.c_str() + s.size()) die("line " + std::to_string(line) + ": " + s + ": invalid float literal", 2);
    return v;
}

std::uint64_t parse_u64(const std::string& s, std::size_t line) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
        die("line " + std::to_string(line) + ": invalid digit found in string", 2);
    return std::stoull(s);
}

std::vector<Pad> parse_script(const std::string& text, std::uint64_t ticks) {
    std::vector<Pad> pads(ticks);
    std::istringstream in(text);
    std::string raw;
    std::size_t n = 0;
    while (std::getline(in, raw)) {
        ++n;
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        const std::string line = trim(raw.substr(0, raw.find('#')));
        if (line.empty()) continue;
        std::istringstream fields(line);
        std::vector<std::string> f;
        for (std::string w; fields >> w;) f.push_back(w);
        if (f.size() != 7 && f.size() != 9) die("line " + std::to_string(n) + ": expected 7 or 9 fields: " + raw, 2);
        const std::uint64_t from = parse_u64(f[0], n);
        const std::uint64_t to = parse_u64(f[1], n);
        unsigned long buttons = 0;
        {
            const bool hex = f[6].rfind("0x", 0) == 0;
            const std::string digits = hex ? f[6].substr(2) : f[6];
            char* end = nullptr;
            buttons = std::strtoul(digits.c_str(), &end, hex ? 16 : 10);
            if (digits.empty() || end != digits.c_str() + digits.size() || buttons > 0xffff)
                die("line " + std::to_string(n) + ": buttons: invalid", 2);
        }
        Pad pad{};
        pad.buttons = static_cast<std::uint16_t>(buttons);
        pad.lx = axis(parse_f32(f[2], n));
        pad.ly = axis(parse_f32(f[3], n));
        pad.rx = axis(parse_f32(f[4], n));
        pad.ry = axis(parse_f32(f[5], n));
        pad.left_trigger = f.size() == 9 ? trigger(parse_f32(f[7], n)) : 0;
        pad.right_trigger = f.size() == 9 ? trigger(parse_f32(f[8], n)) : 0;
        const std::uint64_t last = std::min(to, ticks == 0 ? 0 : ticks - 1);
        for (std::uint64_t t = from; t <= last && ticks > 0; ++t) pads[t] = pad;
    }
    return pads;
}

std::string fmt(float v, bool hex) {
    if (hex) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "%08x", rs::to_bits(v));
        return buf;
    }
    return s3::game::fmt::debug_f32(v);
}

std::string clean(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        if (c == ',' || c == '\n' || c == '"') c = ';';
    return out;
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    bool hex = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--hex") {
            hex = true;
            args.erase(args.begin() + long(i));
            break;
        }
    }
    std::optional<std::string> difficulty;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--difficulty" && i + 1 < args.size()) {
            difficulty = args[i + 1];
            args.erase(args.begin() + long(i), args.begin() + long(i) + 2);
            break;
        }
    }
    if (args.size() != 4) {
        die("usage: s3trace <core.pack> <map.skate|test-world> <input script> <ticks> [--difficulty d] [--hex]", 2);
    }
    std::vector<std::uint8_t> pack;
    if (!read_file(args[0], pack)) die("cannot read pack " + args[0], 1);
    auto vfs = s3::data::Vfs::from_pack(std::move(pack));
    if (!vfs.ok) die("setup failed: " + vfs.error, 1);
    std::optional<s3::data::SkateMap> map;
    if (args[1] != "test-world" && args[1] != "none") {
        std::vector<std::uint8_t> bytes;
        if (!read_file(args[1], bytes)) die("setup failed: cannot read map " + args[1], 1);
        auto parsed = s3::data::SkateMap::parse(bytes.data(), bytes.size());
        if (!parsed.ok) die("setup failed: " + parsed.error, 1);
        map = std::move(parsed.value);
    }
    const std::uint64_t ticks = std::stoull(args[3]);
    std::string script;
    {
        std::vector<std::uint8_t> bytes;
        if (!read_file(args[2], bytes)) die("read input script", 1);
        script.assign(bytes.begin(), bytes.end());
    }
    const auto pads = parse_script(script, ticks);
    auto loaded = Sim::load(vfs.value, "assets", map ? &*map : nullptr, difficulty ? difficulty->c_str() : nullptr);
    if (loaded.is_err()) die("setup failed: " + loaded.unwrap_err(), 1);
    auto sim = std::move(loaded.unwrap());
    map.reset();
    std::fprintf(stderr, "SIMREF time_step=%s\n", s3::game::fmt::debug_f32(sim->time_step()).c_str());

    std::vector<std::string> header = {"tick",         "buttons", "lx", "ly", "rx", "ry", "lt", "rt",
                                       "phys_state",   "phys_state_id", "motion_state", "action_state",
                                       "anim_clip"};
    for (const char* body : {"wheel_rf", "wheel_lf", "wheel_rb", "wheel_lb", "truck_f", "truck_b", "deck"}) {
        for (const char* c : {"px", "py", "pz", "qx", "qy", "qz", "qw", "vx", "vy", "vz", "wx", "wy", "wz"}) {
            header.push_back(std::string(body) + "_" + c);
        }
    }
    for (const char* c : {"rider_px", "rider_py", "rider_pz", "rider_vx", "rider_vy", "rider_vz", "contact_count",
                          "grounded", "wiping_out", "landed", "trick_name", "score_name", "events"}) {
        header.push_back(c);
    }
    std::string out;
    auto join = [](const std::vector<std::string>& v, const char* sep) {
        std::string s;
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (i) s += sep;
            s += v[i];
        }
        return s;
    };
    std::fputs((join(header, ",") + "\n").c_str(), stdout);
    for (std::size_t t = 0; t < pads.size(); ++t) {
        const Pad& pad = pads[t];
        auto step = sim->step(pad);
        if (step.is_err()) {
            std::fflush(stdout);
            die("tick " + std::to_string(t) + " failed: " + step.unwrap_err(), 1);
        }
        const auto& r = step.unwrap();
        char buttons[16];
        std::snprintf(buttons, sizeof buttons, "0x%04x", unsigned(pad.buttons));
        std::vector<std::string> row = {
            std::to_string(r.tick),           buttons,
            std::to_string(pad.lx),           std::to_string(pad.ly),
            std::to_string(pad.rx),           std::to_string(pad.ry),
            std::to_string(pad.left_trigger), std::to_string(pad.right_trigger),
            r.physical_state,                 std::to_string(r.physical_state_id),
            clean(r.motion_state),            clean(r.action_state),
            clean(r.animation_clip),
        };
        for (const auto& b : r.board) {
            for (float v : b.position) row.push_back(fmt(v, hex));
            for (float v : b.orientation) row.push_back(fmt(v, hex));
            for (float v : b.linear_velocity) row.push_back(fmt(v, hex));
            for (float v : b.angular_velocity) row.push_back(fmt(v, hex));
        }
        for (float v : r.rider_root_position) row.push_back(fmt(v, hex));
        for (float v : r.rider_root_velocity) row.push_back(fmt(v, hex));
        row.push_back(std::to_string(r.contact_count));
        row.push_back(r.grounded ? "1" : "0");
        row.push_back(r.wiping_out ? "1" : "0");
        row.push_back(r.landed ? "1" : "0");
        row.push_back(clean(r.trick_name));
        row.push_back(clean(r.score_name));
        row.push_back(clean(join(r.events, "|")));
        std::fputs((join(row, ",") + "\n").c_str(), stdout);
    }
    return 0;
}
