// Port of skate-game/src/difficulty.rs.
#include "s3/game/difficulty.hpp"

#include <cctype>

#include "s3/data/json.hpp"

namespace s3::game::difficulty {

const char* label(Difficulty d) {
    switch (d) {
        case Difficulty::Easy: return "Easy";
        case Difficulty::Normal: return "Normal";
        case Difficulty::Hardcore: return "Hardcore";
    }
    return "";
}

static bool eq_ignore_ascii_case(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

rs::Result<Difficulty, std::string> parse(const std::string& value) {
    for (Difficulty d : ALL) {
        if (eq_ignore_ascii_case(key(d), value)) return d;
    }
    return rs::make_err(fmt::cat("Unknown difficulty \"", value, "\"; expected easy, normal or hardcore"));
}

std::string path(const std::string& root) {
    std::string r = root;
    while (r.size() > 1 && r.back() == '/') r.pop_back();
    const auto slash = r.find_last_of('/');
    const std::string parent = slash == std::string::npos ? std::string() : r.substr(0, slash);
    // Path::parent of a single relative component is "" (Some("")).
    return sd::join_path(parent, "settings/gameplay.json");
}

const char* serde_name(Difficulty d) { return key(d); }

rs::Result<Difficulty, std::string> parse_saved(const std::string& json) {
    auto parsed = sd::JsonDoc::parse(json);
    if (!parsed.ok) return rs::make_err(fmt::cat("Invalid saved gameplay settings: ", parsed.error));
    const auto value = parsed.value.root().get("difficulty");
    if (!value.valid() || !value.is_string())
        return rs::make_err(std::string("Invalid saved gameplay settings: missing field `difficulty`"));
    for (Difficulty d : ALL) {
        if (value.text() == serde_name(d)) return d;
    }
    return rs::make_err(
        fmt::cat("Invalid saved gameplay settings: unknown variant `", std::string(value.text()), "`"));
}

std::string saved_json(Difficulty d) { return fmt::cat("{\"difficulty\":\"", serde_name(d), "\"}"); }

rs::Result<Difficulty, std::string> load(const sd::Vfs& vfs, const std::string& root) {
    const std::string p = path(root);
    if (!vfs.is_file(p)) return Difficulty::Easy;
    auto text = vfs.read_to_string(p);
    if (!text.ok) return rs::make_err(fmt::cat("Cannot read gameplay settings: ", text.error));
    return parse_saved(text.value);
}

}  // namespace s3::game::difficulty
