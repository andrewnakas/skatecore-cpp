// Port of the #[cfg(test)] module in skate-game/src/animation_pose/authored_clips.rs.
#include <functional>
#include <iostream>

#include "assets.hpp"
#include "harness.hpp"
#include "s3/data/json.hpp"
#include "s3/game/animation_pose/authored_clips.hpp"
#include "s3/game/graph_runtime/disk.hpp"

using namespace s3::game::animation_pose::authored_clips;
namespace disk = s3::game::graph_runtime::disk;

namespace {

struct Fixture {
    AnimationFrames frames;
    std::string text;
};

/// Rust reads `CARGO_MANIFEST_DIR/../../assets`; the C++ port uses the asset root and skips when
/// the captured `private/custom/crouch-treflip.json` is not present there.
std::optional<Fixture> fixture(const std::string& root) {
    const s3::data::Vfs vfs = disk::read_files(root, {disk::ON_BOARD_BANK, disk::OFF_BOARD_BANK, disk::AUTHORED_CLIPS});
    const std::string path = s3::data::join_path(root, disk::AUTHORED_CLIPS);
    if (!vfs.is_file(path)) {
        std::cout << "SKIP (no " << path << ")\n";
        return std::nullopt;
    }
    auto banks = s3::data::AnimationBanks::load(vfs, root);
    if (!banks.ok) s3test::fail(__FILE__, __LINE__, banks.error);
    auto frames = AnimationFrames::from_banks(banks.value);
    if (!frames.ok) s3test::fail(__FILE__, __LINE__, frames.error);
    auto text = vfs.read_to_string(path);
    if (!text.ok) s3test::fail(__FILE__, __LINE__, text.error);
    return Fixture{std::move(frames.value), std::move(text.value)};
}

/// Minimal JSON writer standing in for `serde_json::Value` edits: re-serializes `v`, replacing
/// the value at a member path when `replace` returns a non-empty JSON text for it.
std::string json_escape(std::string_view s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out + "\"";
}
using Replace = std::function<std::string(const std::vector<std::string>&)>;
std::string write(s3::data::JsonValue v, std::vector<std::string>& path, const Replace& replace) {
    if (std::string r = replace(path); !r.empty()) return r;
    switch (v.kind()) {
        case s3::data::JsonKind::Null: return "null";
        case s3::data::JsonKind::Bool: return v.as_bool() ? "true" : "false";
        case s3::data::JsonKind::Number: return std::string(v.text());
        case s3::data::JsonKind::String: return json_escape(v.text());
        case s3::data::JsonKind::Array: {
            std::string out = "[";
            std::size_t i = 0;
            for (s3::data::JsonValue x : v) {
                if (i) out += ",";
                path.push_back(std::to_string(i++));
                out += write(x, path, replace);
                path.pop_back();
            }
            return out + "]";
        }
        case s3::data::JsonKind::Object: {
            std::string out = "{";
            bool first = true;
            for (s3::data::JsonValue x : v) {
                if (!first) out += ",";
                first = false;
                path.emplace_back(x.key());
                out += json_escape(x.key()) + ":" + write(x, path, replace);
                path.pop_back();
            }
            return out + "}";
        }
    }
    return "null";
}
std::string edit(const std::string& text, const std::vector<std::string>& target, const std::string& value) {
    auto doc = s3::data::JsonDoc::parse(text);
    if (!doc.ok) s3test::fail(__FILE__, __LINE__, doc.error);
    std::vector<std::string> path;
    return write(doc.value.root(), path, [&](const std::vector<std::string>& p) {
        return p == target ? value : std::string();
    });
}

}  // namespace

S3_TEST(authored_body_preserves_board_trajectory_and_slot_timing) {
    S3_REQUIRE_ASSETS(root);
    auto fx = fixture(root);
    if (!fx) return;
    const AnimationFrames& frames = fx->frames;
    const Replacements replacements = Replacements::parse(fx->text, frames).unwrap();
    CHECK_EQ(replacements._0.size(), std::size_t(7));
    std::size_t board = 0;
    while (frames.bone_names.at(board) != "SKATEBOARD_ROOT") ++board;
    for (const auto& [name, clip] : replacements._0) {
        const ClipFrames& stock = *frames.clip(name).value;
        CHECK_EQ(clip.fps_bits, stock.fps_bits);
        CHECK_EQ(clip.frames.size(), stock.frames.size());
        CHECK(clip.channel_weights == stock.channel_weights);
        bool any_differs = false;
        for (std::size_t i = 0; i < clip.frames.size(); ++i) {
            const auto& a = clip.frames[i];
            const auto& b = stock.frames[i];
            CHECK(a[0] == b[0]);
            CHECK((std::vector<SampleWords>(a.begin() + board, a.begin() + 32) ==
                   std::vector<SampleWords>(b.begin() + board, b.begin() + 32)));
            any_differs = any_differs || a[2] != b[2];
        }
        CHECK(any_differs);
    }
    CHECK(replacements.clip("NOLLIE_360FLIP_D_HIGH_G") == nullptr);
}

S3_TEST(authored_idle_body_loop_closes) {
    S3_REQUIRE_ASSETS(root);
    auto fx = fixture(root);
    if (!fx) return;
    const Replacements replacements = Replacements::parse(fx->text, fx->frames).unwrap();
    for (const char* name : {"R_ANTIC_OLLIE_N_0_CYC", "R_ANTIC_360SHUVIT_N_0_CYC"}) {
        const ClipFrames* c = replacements.clip(name);
        CHECK(c != nullptr);
        const auto& first = c->frames.front();
        const auto& last = c->frames.back();
        CHECK((std::vector<SampleWords>(first.begin() + 1, first.begin() + 25) ==
               std::vector<SampleWords>(last.begin() + 1, last.begin() + 25)));
    }
}

S3_TEST(authored_clip_rejects_bad_timing_and_skeleton) {
    S3_REQUIRE_ASSETS(root);
    auto fx = fixture(root);
    if (!fx) return;
    const std::string wrong_bone = edit(fx->text, {"bone_names", "1"}, "\"WRONG\"");
    CHECK(Replacements::parse(wrong_bone, fx->frames).is_err());
    const std::string wrong_fps = edit(fx->text, {"clips", "360FLIP_D_HIGH_G", "fps"}, "24");
    CHECK(Replacements::parse(wrong_fps, fx->frames).is_err());
}
