// Port of skate-game/src/graph_host/motion_stock_gameplay.rs (out-of-line items).
#include "s3/game/graph_host/motion_stock_gameplay.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/data/result.hpp"

namespace s3::game::graph_host::motion_stock_gameplay {

using s3::animation::skeleton_input::name::encode;

namespace {

std::string normalize_path(std::string_view path) {
    std::string out(path);
    for (char& c : out) {
        if (c == '/') c = '\\';
    }
    return out;
}

std::uint32_t djb2(std::string_view value) {
    std::uint32_t hash = 5381u;
    for (const char c : value) {
        hash = hash * 33u + static_cast<std::uint32_t>(static_cast<unsigned char>(c));
    }
    return hash;
}

/// `a.text(name).map(str::to_owned)`.
std::optional<std::string> text_owned(const Attributes& a, const char* name) {
    const std::string* v = a.text(name);
    return v ? std::optional<std::string>(*v) : std::nullopt;
}

std::string authored_path(const Attributes& a) {
    if (const std::string* v = a.text("path")) return *v;
    if (const std::string* v = a.text("object")) return *v;
    if (const std::string* v = a.text("nameToFind")) return *v;
    return "";
}

std::string required_text(const Attributes& a, const char* name) {
    const std::string* v = a.text(name);
    return v ? *v : std::string();
}

}  // namespace

rs::Result<GrabType, std::string> grab_type_parse(std::string_view value) {
    if (value == "FS") return GrabType::Fs;
    if (value == "BS") return GrabType::Bs;
    if (value == "Nose") return GrabType::Nose;
    if (value == "Tail") return GrabType::Tail;
    return rs::make_err("SetGrabType has unknown grab type " + s3::data::debug_str(std::string(value)));
}

float grab_type_as_value(GrabType self) {
    // TU3 82BB9BD0: zero is no grab; authored types are one through four.
    switch (self) {
        case GrabType::Fs: return 1.0f;
        case GrabType::Bs: return 2.0f;
        case GrabType::Nose: return 3.0f;
        case GrabType::Tail: return 4.0f;
    }
    return 0.0f;
}

std::uint32_t MovingObjectRegistry::register_(std::string path) {
    std::string normalized = normalize_path(path);
    const std::uint32_t hash = djb2(normalized);
    objects[hash] = std::move(normalized);
    return hash;
}

rs::Result<rs::Unit, std::string> MovingObjectRegistry::begin(std::string_view path) {
    const std::uint32_t hash = djb2(normalize_path(path));
    if (objects.count(hash) == 0) {
        return rs::make_err("MovingObject resource is not registered: " + std::string(path));
    }
    active_ = hash;
    return rs::Unit{};
}

rs::Result<bool, std::string> MovingObjectRegistry::update(std::string_view path) {
    const std::uint32_t hash = djb2(normalize_path(path));
    if (objects.count(hash) == 0) {
        return rs::make_err("MovingObject update references unknown resource: " + std::string(path));
    }
    return active_ == std::optional<std::uint32_t>(hash);
}

std::string_view select_grab_score(std::string_view base, std::array<std::optional<std::string_view>, 4> directions,
                                   float x, float y) {
    const auto& [up, left, down, right] = directions;
    if (rs::sqrt(x * x + y * y) <= 0.5f) {
        return base;
    }
    const float angle = rs::rem_euclid(rs::atan2(y, x), rs::f32::consts::TAU);
    struct Window {
        const std::optional<std::string_view>* name;
        float start;
        float end;
    };
    const std::array<Window, 4> windows = {
        Window{&up, right ? 44.0f : 359.0f, left ? 134.0f : 181.0f},
        Window{&left, up ? 44.0f : 89.0f, down ? 226.0f : 271.0f},
        Window{&down, left ? 224.0f : 179.0f, right ? 316.0f : 1.0f},
        Window{&right, down ? 314.0f : 269.0f, up ? 46.0f : 91.0f},
    };
    constexpr float degrees = rs::f32::consts::PI / 180.0f;
    for (const Window& window : windows) {
        const float start = window.start * degrees;
        const float end = window.end * degrees;
        const bool inside = end < start ? (angle >= start || angle <= end) : (angle >= start && angle <= end);
        if (inside && window.name->has_value()) {
            return **window.name;
        }
    }
    return base;
}

bool Operation::recognizes(std::string_view name) {
    return name == "MatchAirTime" || name == "UpdateStandingOnCar" || name == "InitMovingObjects" ||
           name == "MovingObject" || name == "SetGrabType" || name == "JumpInto" || name == "ScoringHandPlants" ||
           name == "SetHandPlantAnticLength" || name == "ScoringGrabs" || name == "AirDismounting" ||
           name == "TweakProject" || name == "FootPlantAbsorb" || name == "FingerFlipOut" ||
           name == "EnterSkitchingBehaviour" || name == "SkitchingBehaviour" ||
           name == "SkitchShimmyingBehaviour" || name == "SetManualAngle" || name == "HippyJumpAntic" ||
           name == "LandOnBoard";
}

Operation Operation::parse(const Attributes& a) {
    const std::string* name_ptr = a.text("name");
    const std::string name = name_ptr ? *name_ptr : std::string();
    if (name == "MatchAirTime") return MatchAirTime{};
    if (name == "UpdateStandingOnCar") return UpdateStandingOnCar{};
    if (name == "InitMovingObjects") return InitMovingObjects{authored_path(a)};
    if (name == "MovingObject") return MovingObject{authored_path(a)};
    if (name == "SetGrabType") return SetGrabType{required_text(a, "grab")};
    if (name == "JumpInto") {
        const std::string* attribute = a.text("attribute");
        if (!attribute) attribute = a.text("attr");
        return JumpInto{encode(attribute ? std::string_view(*attribute) : std::string_view("JumpInto"))};
    }
    if (name == "ScoringHandPlants") return ScoringHandPlants{};
    if (name == "SetHandPlantAnticLength") return SetHandPlantAnticLength{};
    if (name == "ScoringGrabs") {
        ScoringGrabs out;
        out.grab_name = required_text(a, "grabName");
        out.up = text_owned(a, "up");
        out.left = text_owned(a, "left");
        out.down = text_owned(a, "down");
        out.right = text_owned(a, "right");
        const std::string* intent_x = a.text("intentX");
        if (!intent_x) intent_x = a.text("angle");
        out.intent_x = intent_x ? *intent_x : std::string();
        const std::string* intent_y = a.text("intentY");
        if (!intent_y) intent_y = a.text("magnitude");
        out.intent_y = intent_y ? *intent_y : std::string();
        out.invert_y = a.boolean_byte("invertY", 0) != 0;
        out.polar = a.text("intentX") == nullptr;
        return out;
    }
    if (name == "AirDismounting") return AirDismounting{};
    if (name == "TweakProject") {
        // TU3 factory 82BC9510 binds these names directly.
        return TweakProject{"TweakX", "TweakY", encode("tweak_x"), encode("tweak_y")};
    }
    if (name == "FootPlantAbsorb") return FootPlantAbsorb{};
    if (name == "FingerFlipOut") {
        //Factory82BC7C18 binds authored grabintent and literal Grabbing.
        return FingerFlipOut{required_text(a, "grabintent")};
    }
    if (name == "EnterSkitchingBehaviour") return EnterSkitchingBehaviour{};
    if (name == "SkitchingBehaviour") return SkitchingBehaviour{};
    if (name == "SkitchShimmyingBehaviour") return SkitchShimmyingBehaviour{};
    if (name == "SetManualAngle") {
        // Factory82BC9678/84 uses literal8231D5C8, no authored override.
        return SetManualAngle{encode("manual_angle")};
    }
    if (name == "HippyJumpAntic") return HippyJumpAntic{};
    if (name == "LandOnBoard") return LandOnBoard{};
    S3_PANIC("internal error: entered unreachable code: unrecognized stock gameplay operation");
}

}  // namespace s3::game::graph_host::motion_stock_gameplay
