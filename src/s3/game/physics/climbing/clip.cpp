// Port of skate-game/src/physics/climbing/clip.rs
//! Optional authored clips, separate from the SHA-bound stock animation banks.
#include "s3/game/physics/climbing/clip.hpp"

#include <algorithm>

#include "s3/data/json.hpp"
#include "s3/game/glam_ext.hpp"

namespace s3::game::physics::climbing::clip {

namespace {

using sd::JsonFields;
using sd::JsonValue;

/// serde `Deserialize for [f32; 10]` (exact length).
sd::Result<std::array<float, 10>> frame_sample(JsonValue v) {
    if (!v.is_array()) return sd::Error{"invalid type: " + v.type_name() + ", expected an array of length 10"};
    if (v.size() != 10) return sd::Error{"invalid length " + std::to_string(v.size()) + ", expected an array of length 10"};
    std::array<float, 10> out{};
    std::size_t i = 0;
    for (JsonValue x : v) {
        auto f = x.to_f32();
        if (!f.ok) return sd::Error{f.error};
        out[i++] = f.value;
    }
    return out;
}

sd::Result<Clip> parse_clip(JsonValue v) {
    JsonFields o(v, {"name", "fps", "names", "parents", "frames"}, false);
    if (!o.ok()) return sd::Error{o.error()};
    Clip c;
    {
        auto f = o.require("name");
        if (!f.ok) return sd::Error{f.error};
        auto s = f.value.to_string();
        if (!s.ok) return sd::Error{s.error};
        c.name = s.value;
    }
    {
        auto f = o.require("fps");
        if (!f.ok) return sd::Error{f.error};
        auto s = f.value.to_f32();
        if (!s.ok) return sd::Error{s.error};
        c.fps = s.value;
    }
    {
        auto f = o.require("names");
        if (!f.ok) return sd::Error{f.error};
        auto s = sd::json_string_vec(f.value);
        if (!s.ok) return sd::Error{s.error};
        c.names = std::move(s.value);
    }
    {
        auto f = o.require("parents");
        if (!f.ok) return sd::Error{f.error};
        auto s = sd::json_i32_vec(f.value);
        if (!s.ok) return sd::Error{s.error};
        c.parents = std::move(s.value);
    }
    {
        auto f = o.require("frames");
        if (!f.ok) return sd::Error{f.error};
        auto s = sd::json_vec<std::vector<std::array<float, 10>>>(f.value, [](JsonValue frame) {
            return sd::json_vec<std::array<float, 10>>(frame, frame_sample);
        });
        if (!s.ok) return sd::Error{s.error};
        c.frames = std::move(s.value);
    }
    return c;
}

sd::Result<File> parse_file(std::string_view text) {
    auto doc = sd::JsonDoc::parse(text);
    if (!doc.ok) return sd::Error{doc.error};
    JsonFields o(doc.value.root(), {"version", "clips"}, false);
    if (!o.ok()) return sd::Error{o.error()};
    File file;
    {
        auto f = o.require("version");
        if (!f.ok) return sd::Error{f.error};
        auto s = f.value.to_u32();
        if (!s.ok) return sd::Error{s.error};
        file.version = s.value;
    }
    {
        auto f = o.require("clips");
        if (!f.ok) return sd::Error{f.error};
        auto s = sd::json_vec<Clip>(f.value, parse_clip);
        if (!s.ok) return sd::Error{s.error};
        file.clips = std::move(s.value);
    }
    return file;
}

}  // namespace

rs::Result<std::optional<Clips>, std::string> Clips::load(const sd::Vfs& vfs, const std::string& root) {
    const std::string path = sd::join_path(root, "private/custom/climbing.json");
    if (!vfs.is_file(path)) return std::optional<Clips>();  // ErrorKind::NotFound
    auto bytes = vfs.read(path);
    if (!bytes.ok) return rs::make_err(fmt::cat(path, ": ", bytes.error));
    const std::string_view text(reinterpret_cast<const char*>(bytes.value.data), bytes.value.size);
    auto parsed = parse_file(text);
    if (!parsed.ok) return rs::make_err(fmt::cat("Climbing clips: ", parsed.error));
    File file = std::move(parsed.value);
    if (file.version != 1) return rs::make_err(std::string("Unsupported climbing clip version"));
    for (const auto& c : file.clips) {
        S3G_CHECK(c.validate());
    }
    auto take = [&](const std::string& name) -> rs::Result<Clip, std::string> {
        for (std::size_t i = 0; i < file.clips.size(); ++i) {
            if (file.clips[i].name == name) {
                Clip c = std::move(file.clips[i]);
                file.clips.erase(file.clips.begin() + std::ptrdiff_t(i));
                return c;
            }
        }
        return rs::make_err(fmt::cat("Missing climbing clip ", name));
    };
    S3G_TRY(Clip reach, take("reach"));
    S3G_TRY(Clip mantle, take("mantle"));
    if (reach.names != mantle.names || reach.parents != mantle.parents) {
        return rs::make_err(std::string("Climbing clips must share a skeleton"));
    }
    return std::optional<Clips>(Clips{std::move(reach), std::move(mantle)});
}

rs::Result<rs::Unit, std::string> Clip::validate() const {
    bool valid = rs::is_finite(fps) && fps > 0.0f && fps <= 240.0f && frames.size() >= 2 && !names.empty() &&
                 parents.size() == names.size();
    if (valid) {
        for (std::size_t i = 0; i < parents.size(); ++i) {
            const std::int32_t p = parents[i];
            if (!(p >= -1 && p < std::int32_t(i))) {
                valid = false;
                break;
            }
        }
    }
    if (valid) {
        for (std::size_t i = 0; i < names.size() && valid; ++i) {
            for (std::size_t j = 0; j < i; ++j) {
                if (names[j] == names[i]) {
                    valid = false;
                    break;
                }
            }
        }
    }
    if (valid) {
        for (const auto& f : frames) {
            if (f.size() != names.size()) {
                valid = false;
                break;
            }
            for (const auto& s : f) {
                bool ok = true;
                for (float v : s) ok = ok && rs::is_finite(v);
                for (std::size_t k = 0; k < 3; ++k) ok = ok && s[k] > 0.0001f;
                ok = ok && rs::abs(Quat::from_array({s[3], s[4], s[5], s[6]}).length() - 1.0f) < 0.01f;
                if (!ok) {
                    valid = false;
                    break;
                }
            }
            if (!valid) break;
        }
    }
    if (!valid) return rs::make_err(fmt::cat("Invalid climbing clip ", name));
    for (const char* bone : {"HIPS", "SPINE3", "LEFTSHOULDER", "RIGHTSHOULDER", "LEFTARM", "RIGHTARM", "LEFTFOREARM",
                             "RIGHTFOREARM", "LEFTHAND", "RIGHTHAND", "LEFTFOOT", "RIGHTFOOT", "SKATEBOARD_ROOT"}) {
        if (std::find(names.begin(), names.end(), bone) == names.end()) {
            return rs::make_err(fmt::cat("Missing climbing bone ", bone));
        }
    }
    return rs::Unit{};
}

float Clip::duration() const { return float(frames.size() - 1) / fps; }

std::vector<Transform> Clip::sample(float time) const {
    const float frame = rs::clamp(time * fps, 0.0f, float(frames.size() - 1));
    const std::size_t i = rs::as_usize(rs::floor(frame));
    const std::size_t j = std::min(i + 1, frames.size() - 1);
    std::vector<Transform> out;
    const auto& a = frames[i];
    const auto& b = frames[j];
    const std::size_t n = std::min(a.size(), b.size());
    out.reserve(n);
    for (std::size_t k = 0; k < n; ++k) out.push_back(blend(decode(a[k]), decode(b[k]), rs::fract(frame)));
    return out;
}

std::vector<Mat4> Clip::globals(const std::vector<Transform>& locals) const {
    std::vector<Mat4> result;
    result.reserve(locals.size());
    for (std::size_t i = 0; i < locals.size(); ++i) {
        const Mat4 m = locals[i].to_matrix();
        if (parents[i] < 0) {
            result.push_back(m);
        } else {
            const Mat4 parent = result[std::size_t(parents[i])];
            result.push_back(parent * m);
        }
    }
    return result;
}

std::size_t Clip::index(std::string_view name_) const {
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name_) return i;
    }
    S3_PANIC("called `Option::unwrap()` on a `None` value");
}

Vec3 Clip::hands(const std::vector<Mat4>& g) const {
    return (glam::truncate(g[index("LEFTHAND")].w_axis) + glam::truncate(g[index("RIGHTHAND")].w_axis)) * 0.5f;
}

Vec3 Clip::feet(const std::vector<Mat4>& g) const {
    return (glam::truncate(g[index("LEFTFOOT")].w_axis) + glam::truncate(g[index("RIGHTFOOT")].w_axis)) * 0.5f -
           glam::VEC3_Y * 0.11f;
}

Transform decode(const std::array<float, 10>& s) {
    Transform t;
    t.scale = Vec3::new_(s[0], s[1], s[2]);
    t.rotation = Quat::from_xyzw(s[3], s[4], s[5], s[6]).normalize();
    t.translation = Vec3::new_(s[7], s[8], s[9]);
    return t;
}

Transform blend(Transform a, Transform b, float t) {
    Transform out;
    out.translation = glam::lerp(a.translation, b.translation, t);
    out.rotation = glam::slerp(a.rotation, b.rotation, t);
    out.scale = glam::lerp(a.scale, b.scale, t);
    return out;
}

}  // namespace s3::game::physics::climbing::clip
