// Port of skate-game/src/input/gesture_input.rs.
#include "s3/game/input/gesture_input.hpp"

#include "s3/data/collections.hpp"
#include "s3/data/gesture_patterns.hpp"
#include "s3/game/common.hpp"

namespace s3::game::input::gesture_input {

namespace {
bool eq_ignore_ascii_case(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

/// `u8::from_str_radix(text, 16)` with Rust's ParseIntError messages.
rs::Result<std::uint8_t, std::string> parse_u8_hex(const std::string& text) {
    if (text.empty()) return rs::make_err(std::string("cannot parse integer from empty string"));
    std::size_t start = 0;
    if (text[0] == '+') {
        start = 1;
        if (text.size() == 1) return rs::make_err(std::string("invalid digit found in string"));
    }
    std::uint32_t value = 0;
    for (std::size_t i = start; i < text.size(); ++i) {
        const char c = text[i];
        std::uint32_t d;
        if (c >= '0' && c <= '9') d = std::uint32_t(c - '0');
        else if (c >= 'a' && c <= 'f') d = std::uint32_t(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = std::uint32_t(c - 'A' + 10);
        else return rs::make_err(std::string("invalid digit found in string"));
        value = value * 16u + d;
        if (value > 255u) return rs::make_err(std::string("number too large to fit in target type"));
    }
    return static_cast<std::uint8_t>(value);
}
}  // namespace

rs::Result<GestureInput, std::string> GestureInput::load(const ::s3::data::Vfs& vfs, const std::string& asset_root) {
    S3G_TRY(auto data, ::s3::data::Collections::load(vfs, asset_root));
    auto misses = [&](const char* key) -> rs::Result<std::uint8_t, std::string> {
        S3G_TRY(const ::s3::data::Field* field,
                data.field("recognizer", key, "NumTicksPatternNotInRangeBeforeCulling"));
        if (field->type_name != "EA::Reflection::UInt8")
            return rs::make_err(std::string("Recognizer culling field must be UInt8"));
        return parse_u8_hex(field->data);
    };
    GestureInput out;
    // Original constructor82695A68 insertion order; each file competes
    // internally, and all winners are delivered to the listener in order.
    const std::pair<std::size_t, const char*> files[] = {
        {1, "skater.pat"},     {1, "skater90.pat"},         {1, "skaterN90.pat"}, {1, "skater_air.pat"},
        {1, "skater_fingerflip.pat"}, {0, "skaterls.pat"}, {0, "skaterstep.pat"},
    };
    for (const auto& [stick, file] : files) {
        const std::string path =
            ::s3::data::join_path(::s3::data::join_path(asset_root, "private/stock/data/joystick"), file);
        S3G_TRY(auto loaded, ::s3::data::gesture_patterns::load(vfs, path));
        std::vector<::s3::input::gesture::Pattern> patterns;
        patterns.reserve(loaded.size());
        for (auto& p : loaded) patterns.push_back(::s3::input::gesture::Pattern{p.name, p.points, p.tolerance_squared});
        S3G_TRY(auto recognizer, ::s3::input::gesture::Recognizer::new_(std::move(patterns)));
        out.recognizers.emplace_back(stick, std::move(recognizer));
    }
    S3G_TRY(auto left, misses("left_stick"));
    S3G_TRY(auto right, misses("right_stick"));
    out.maximum_misses = {left, right};
    out.held_pattern = std::nullopt;
    return out;
}

void GestureInput::publish(std::array<std::array<float, 2>, 2> axes, std::uint32_t difficulty, std::uint32_t flags,
                           std::uint32_t physical_state, ::s3::graph::intents::IntentMap& ag) {
    // Native manager negates mapped Y. Component deadzone0.1 is initialized
    // by82F75F60 from820641A8, separately from cInputMap's own deadzones.
    std::array<std::array<float, 2>, 2> samples{};
    for (std::size_t s = 0; s < 2; ++s) {
        const std::array<float, 2> v = {axes[s][0], -axes[s][1]};
        for (std::size_t i = 0; i < 2; ++i) samples[s][i] = rs::abs(v[i]) < 0.1f ? 0.0f : v[i];
    }
    std::vector<std::pair<std::string, float>> events;
    bool held = false;
    for (std::size_t stick : {std::size_t(1), std::size_t(0)}) {
        // Held checks precede *all* matches on this stick.
        for (auto& [id, recognizer] : recognizers) {
            if (id != stick) continue;
            if (auto index = recognizer.held(samples[stick])) {
                if (held_pattern && eq_ignore_ascii_case(*held_pattern, recognizer.patterns()[*index].name)) {
                    held = true;
                }
            }
        }
        for (auto& [id, recognizer] : recognizers) {
            if (id != stick) continue;
            ::s3::input::gesture::Settings settings;
            settings.maximum_misses = maximum_misses[stick];
            settings.difficulty = difficulty;
            if (auto result = recognizer.sample(samples[stick], settings)) {
                std::string name = recognizer.patterns()[result->pattern].name;
                // Listener retains only these four hold-capable events.
                if (name == "Kickflip" || name == "Heelflip" || name == "N_Kickflip" || name == "N_Heelflip") {
                    held_pattern = name;
                }
                events.emplace_back(name, result->strength);
            }
        }
    }
    if (held) ag.insert("HoldPattern", 1.0f);
    for (const auto& [name, strength] : events) {
        if (permitted(name, flags, physical_state, ag)) {
            ag.insert("Trick", 1.0f);
            ag.insert(name, 1.0f);
            ag.insert("GestureSpeed", strength);
        }
    }
}

bool permitted(std::string_view name, std::uint32_t flags, std::uint32_t state,
               const ::s3::graph::intents::IntentMap& ag) {
    auto bit = [&](std::uint32_t n) { return (flags & (1u << n)) != 0u; };
    auto member = [&](std::initializer_list<const char*> names) {
        for (const char* n : names)
            if (eq_ignore_ascii_case(n, name)) return true;
        return false;
    };
    if (bit(5) && member({"FingerFlip", "FS_Varial", "BS_Varial"})) return false;
    if (bit(4) && ((state == 200 || state == 201) ||
                   member({"L_F_Kickflip", "L_B_Kickflip", "L_F_Heelflip", "L_B_Heelflip", "L_FS_Shuvit",
                           "L_BS_Shuvit"}))) {
        return false;
    }
    if (member({"FrontFlip", "BackFlip"}) &&
        (!(ag.contains_key("RightAirGrab") || ag.contains_key("LeftAirGrab")) || ag.contains_key("LeftPush") ||
         ag.contains_key("RightPush") || bit(3))) {
        return false;
    }
    if (bit(8) && member({"SlideFs180", "SlideBs180"})) return false;
    if (bit(12) && !member({"Ollie", "Kickflip", "Heelflip", "Hardflip", "InwardHeelflip", "VarialKickflip",
                            "VarialHeelflip", "PopShuvit", "FSPopShuvit", "360PopShuvit", "FS360PopShuvit"})) {
        return false;
    }
    return true;
}

}  // namespace s3::game::input::gesture_input
