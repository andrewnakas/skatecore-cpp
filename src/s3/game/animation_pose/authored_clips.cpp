// Port of skate-game/src/animation_pose/authored_clips.rs.
#include "s3/game/animation_pose/authored_clips.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "s3/core/animation/pose_add.hpp"
#include "s3/data/json.hpp"
#include "s3/game/animation_pose.hpp"
#include "s3/game/graph_runtime/disk.hpp"

namespace s3::game::animation_pose::authored_clips {

using glam::Mat4;
using glam::Quat;
using glam::Vec3;
using s3::data::JsonDoc;
using s3::data::JsonFields;
using s3::data::JsonValue;

namespace {

/// serde `AuthoredClip` (deny_unknown_fields).
struct AuthoredClip {
    float fps = 0.0f;
    std::vector<std::vector<std::array<float, 16>>> frames;
};

/// serde `File` (deny_unknown_fields).
struct File {
    std::uint32_t version = 0;
    std::vector<std::string> bone_names;
    std::map<std::string, AuthoredClip> clips;
};

s3::data::Result<AuthoredClip> parse_clip(JsonValue v) {
    JsonFields f(v, {"fps", "frames"}, true);
    if (!f.ok()) return s3::data::Error{f.error()};
    AuthoredClip clip;
    S3_TRY(JsonValue fps, f.require("fps"));
    S3_TRY(clip.fps, fps.to_f32());
    S3_TRY(JsonValue frames, f.require("frames"));
    using Row = std::vector<std::array<float, 16>>;
    S3_TRY(clip.frames, s3::data::json_vec<Row>(frames, [](JsonValue row) {
               return s3::data::json_vec<std::array<float, 16>>(
                   row, [](JsonValue m) -> s3::data::Result<std::array<float, 16>> {
                       if (!m.is_array()) return s3::data::Error{"invalid type: " + m.type_name() + ", expected an array of length 16"};
                       if (m.size() != 16) return s3::data::Error{"invalid length " + std::to_string(m.size()) + ", expected an array of length 16"};
                       std::array<float, 16> out{};
                       std::size_t i = 0;
                       for (JsonValue x : m) {
                           S3_TRY(out[i], x.to_f32());
                           ++i;
                       }
                       return out;
                   });
           }));
    return clip;
}

s3::data::Result<File> parse_file(std::string_view text) {
    S3_TRY(JsonDoc doc, JsonDoc::parse(text));
    JsonFields f(doc.root(), {"version", "bone_names", "clips"}, true);
    if (!f.ok()) return s3::data::Error{f.error()};
    File file;
    S3_TRY(JsonValue version, f.require("version"));
    S3_TRY(file.version, version.to_u32());
    S3_TRY(JsonValue bone_names, f.require("bone_names"));
    S3_TRY(file.bone_names, s3::data::json_string_vec(bone_names));
    S3_TRY(JsonValue clips, f.require("clips"));
    if (!clips.is_object()) return s3::data::Error{"invalid type: " + clips.type_name() + ", expected a map"};
    for (JsonValue member : clips) {
        S3_TRY(AuthoredClip clip, parse_clip(member));
        // BTreeMap insertion: a repeated key keeps the last value.
        file.clips[std::string(member.key())] = std::move(clip);
    }
    return file;
}

}  // namespace

rs::Result<Replacements, std::string> Replacements::load(const s3::data::Vfs& vfs, const std::string& root,
                                                         const AnimationFrames& frames) {
    const std::string path = s3::data::join_path(root, "private/custom/crouch-treflip.json");
    if (!vfs.is_file(path)) return Replacements{};
    auto text = vfs.read_to_string(path);
    if (!text.ok) return rs::make_err(path + ": " + text.error);
    auto parsed = parse(text.value, frames);
    if (parsed.is_err()) return rs::make_err(path + ": " + parsed.unwrap_err());
    return std::move(parsed.unwrap());
}

rs::Result<Replacements, std::string> Replacements::load(const std::string& root, const AnimationFrames& frames) {
    return load(graph_runtime::disk::read_files(root, {graph_runtime::disk::AUTHORED_CLIPS}), root, frames);
}

rs::Result<Replacements, std::string> Replacements::parse(std::string_view text, const AnimationFrames& frames) {
    auto file_result = parse_file(text);
    if (!file_result.ok) return rs::make_err(file_result.error);
    File file = std::move(file_result.value);
    if (file.version != 1 || file.bone_names != frames.bone_names || file.clips.size() > 32) {
        return rs::make_err(std::string("Authored clip format or skeleton mismatch"));
    }
    auto bind_result = frames.named_pose("RIG_TPOSE");
    if (!bind_result.ok) return rs::make_err(bind_result.error);
    const std::vector<SampleWords>& bind = bind_result.value->samples;
    std::string missing;
    auto index = [&](const char* name) -> std::optional<std::size_t> {
        auto it = std::find(frames.bone_names.begin(), frames.bone_names.end(), name);
        if (it == frames.bone_names.end()) {
            missing = std::string("Missing ") + name;
            return std::nullopt;
        }
        return static_cast<std::size_t>(it - frames.bone_names.begin());
    };
#define S3_INDEX(var, name)                                     \
    const auto var##_opt = index(name);                         \
    if (!var##_opt) return rs::make_err(missing);               \
    const std::size_t var = *var##_opt
    S3_INDEX(hips, "HIPS");
    S3_INDEX(board, "SKATEBOARD_ROOT");
    S3_INDEX(right_toe_helper, "RIGHTTOEBASE_REPARENTED");
    S3_INDEX(right_toe, "RIGHTTOEBASE");
    S3_INDEX(left_toe_helper, "LEFTTOEBASE_REPARENTED");
    S3_INDEX(left_toe, "LEFTTOEBASE");
    S3_INDEX(right_hand_helper, "RIGHTHAND_REPARENTED");
    S3_INDEX(right_hand, "RIGHTHAND");
    S3_INDEX(left_hand_helper, "LEFTHAND_REPARENTED");
    S3_INDEX(left_hand, "LEFTHAND");
#undef S3_INDEX
    const std::array<std::pair<std::size_t, std::size_t>, 4> helpers = {
        std::make_pair(right_toe_helper, right_toe),
        std::make_pair(left_toe_helper, left_toe),
        std::make_pair(right_hand_helper, right_hand),
        std::make_pair(left_hand_helper, left_hand),
    };
    std::size_t first_helper = helpers[0].first;
    for (const auto& p : helpers) first_helper = std::min(first_helper, p.first);

    std::map<std::string, ClipFrames> output;
    for (auto& [name, authored] : file.clips) {
        const bool allowed = name == "R_ANTIC_OLLIE_N_0_INTO" || name == "R_ANTIC_OLLIE_N_0_CYC" ||
                             name == "R_ANTIC_360SHUVIT_N_0_CYC" || name == "360FLIP_D_HIGH_G" ||
                             name == "360FLIP_D_HIGH_A" || name == "360FLIP_D_LOW_G" || name == "360FLIP_D_LOW_A";
        if (!allowed) return rs::make_err("Unsupported authored slot " + name);
        auto stock_result = frames.clip(name);
        if (!stock_result.ok) return rs::make_err(stock_result.error);
        const ClipFrames& stock = *stock_result.value;
        if (authored.fps != rs::from_bits(stock.fps_bits) || authored.frames.size() != stock.frames.size()) {
            return rs::make_err(name + ": timing differs from stock slot");
        }
        std::vector<std::vector<SampleWords>> baked = stock.frames;
        for (std::size_t r = 0; r < baked.size(); ++r) {
            std::vector<SampleWords>& row = baked[r];
            const std::vector<std::array<float, 16>>& absolute = authored.frames[r];
            if (absolute.size() != frames.bone_names.size()) return rs::make_err(name + ": missing bones");
            std::vector<Mat4> globals;
            globals.reserve(absolute.size());
            for (const std::array<float, 16>& values : absolute) {
                const Mat4 m = Mat4::from_cols_array(values);
                const auto [scale, rotation, translation_unused] = m.to_scale_rotation_translation();
                (void)translation_unused;
                if (!m.is_finite() || m.determinant() <= 0.0f || scale.min_element() < 0.9f ||
                    scale.max_element() > 1.1f || !rotation.is_finite() || rs::abs(m.w_axis.w - 1.0f) > 0.001f ||
                    rs::abs(m.x_axis.w) + rs::abs(m.y_axis.w) + rs::abs(m.z_axis.w) > 0.001f) {
                    return rs::make_err(name + ": invalid joint matrix");
                }
                globals.push_back(m);
            }
            // Reconstruct the stock board hierarchy to express helper targets in its moving space.
            std::vector<Mat4> stock_globals(row.size(), Mat4::IDENTITY);
            for (std::size_t bone = 1; bone < row.size(); ++bone) {
                const std::int32_t parent = frames.parents[bone];
                const Mat4 local = matrix(s3::animation::pose_add::add(sqt(row[bone]), sqt(bind[bone]), true));
                stock_globals[bone] = parent > 0 ? stock_globals[static_cast<std::size_t>(parent)] * local : local;
            }
            for (std::size_t bone = board; bone < first_helper; ++bone) globals[bone] = stock_globals[bone];
            for (const auto& [helper, joint] : helpers) globals[helper] = globals[joint];
            std::vector<std::size_t> rebased;
            for (std::size_t bone = hips; bone < board; ++bone) rebased.push_back(bone);
            for (const auto& p : helpers) rebased.push_back(p.first);
            for (std::size_t bone : rebased) {
                const std::int32_t parent = frames.parents[bone];
                const Mat4 local =
                    parent > 0 ? globals[static_cast<std::size_t>(parent)].inverse() * globals[bone] : globals[bone];
                row[bone] = words(remove_reference(local, sqt(bind[bone])));
            }
        }
        ClipFrames clip;
        clip.name = name;
        clip.source_offset = stock.source_offset;
        clip.fps_bits = stock.fps_bits;
        clip.loop_translation_bits = stock.loop_translation_bits;
        clip.loop_rotation_bits = stock.loop_rotation_bits;
        clip.channel_animation = stock.channel_animation;
        clip.channel_weights = stock.channel_weights;
        clip.frames = std::move(baked);
        output.insert_or_assign(name, std::move(clip));
    }
    return Replacements{std::move(output)};
}

Mat4 matrix(const Sqt& s) {
    return Mat4::from_scale_rotation_translation(Vec3::from_slice(s.scale), Quat::from_array(s.rotation),
                                                 Vec3::from_slice(s.translation));
}

Sqt remove_reference(const Mat4& local, const Sqt& reference) {
    const auto [scale_, rotation, translation_] = local.to_scale_rotation_translation();
    const Quat inverse = Quat::from_array(reference.rotation).inverse();
    const Vec3 scale = scale_ / Vec3::from_slice(reference.scale);
    // AddSQT rotates translation without applying the reference scale.
    const Vec3 translation = inverse * (translation_ - Vec3::from_slice(reference.translation));
    Sqt out;
    out.scale = {scale.x, scale.y, scale.z, 1.0f};
    out.rotation = (inverse * rotation).normalize().to_array();
    out.translation = {translation.x, translation.y, translation.z, 1.0f};
    return out;
}

SampleWords words(const Sqt& s) {
    return rs::to_bits(std::array<float, 10>{s.scale[0], s.scale[1], s.scale[2], s.rotation[0], s.rotation[1],
                                             s.rotation[2], s.rotation[3], s.translation[0], s.translation[1],
                                             s.translation[2]});
}

}  // namespace s3::game::animation_pose::authored_clips
