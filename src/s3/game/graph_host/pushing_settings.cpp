// Port of skate-game/src/graph_host/pushing_settings.rs.
#include "s3/game/graph_host/pushing_settings.hpp"

#include <array>
#include <optional>

#include "s3/core/animation/clip_clock.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/game/graph_runtime/disk.hpp"

namespace s3::game::graph_host::pushing_settings {

using s3::animation::skeleton_input::name::encode;
using s3::point_graph::PointGraph;

rs::Result<PushTreeMetadata, std::string> metadata(const s3::data::AnimationMetadata& self,
                                                   std::string_view tree_name, std::uint32_t attribute_mask) {
    auto clip_result = self.clip(std::string(tree_name));
    if (!clip_result.ok) return rs::make_err(clip_result.error);
    const s3::data::ClipMetadata& clip = *clip_result.value;
    const float frames = rs::from_bits(clip.frames_bits);
    const float fps = rs::from_bits(clip.fps_bits);
    const float base_speed = rs::from_bits(clip.base_speed_bits);
    // Init827B8AB0 sets speed1/time0, then recomputes length with this
    // specific multiply order (different from later SetSpeed).
    const float length = (frames - 1.0f) / ((1.0f * base_speed) * fps);
    s3::animation::clip_clock::ClipClock clock{};
    clock.frames = frames;
    clock.fps = fps;
    clock.base_speed = base_speed;
    clock.speed = 1.0f;
    clock.length = length;
    clock.time = 0.0f;
    clock.previous_time = 0.0f;
    clock.loops_since_evaluation = 0;
    clock.looping = (clip.flags_word & 0x10000000u) != 0;
    clock.phase_controlled = (clip.flags_word & 0x40000000u) != 0;
    std::vector<PushTreeAttribute> attributes;
    // GetAttributes82D25E30 appends in stored order. Untimed records
    // bypass both mask tests; timed records use GetAttributeStatus.
    for (const s3::data::ClipAttribute& attribute : clip.attributes) {
        const float begin = rs::from_bits(attribute.begin_bits);
        if (begin != -1.0f) {
            const std::uint32_t status =
                static_cast<std::uint32_t>(clock.attribute_status(begin, rs::from_bits(attribute.end_bits)));
            if ((attribute_mask & 0x1cu & status) == 0 || (attribute_mask & 3u & status) == 0) continue;
        }
        // Attribute::Init82D164F8 copies lane0 for types0/1/3. Type2
        // samples a curve; unknown types leave lane0 untouched. Neither
        // can be replaced by the first serialized word as a fallback.
        if (!(attribute.type_id == 0 || attribute.type_id == 1 || attribute.type_id == 3)) {
            return rs::make_err(std::string(tree_name) + ": attribute " + attribute.name + " needs type" +
                                std::to_string(attribute.type_id) + " evaluation");
        }
        attributes.push_back(PushTreeAttribute{attribute.name, rs::from_bits(attribute.payload_words.at(0))});
    }
    return PushTreeMetadata{length, std::move(attributes)};
}

namespace {

rs::Result<PointGraph<8>, std::string> curve(const s3::data::Collections& data, const std::string& name) {
    auto words = data.words<20>("anim_motion", "pushing", name);
    if (!words.ok) return rs::make_err(words.error);
    PointGraph<8> out;
    out.x = rs::from_fn<8>([&](std::size_t i) { return rs::from_bits(words.value[4 + i]); });
    out.y = rs::from_fn<8>([&](std::size_t i) { return rs::from_bits(words.value[12 + i]); });
    return out;
}

rs::Result<PushAttributes, std::string> attributes(PushTreeSource& trees, const std::array<const char*, 4>& names) {
    std::vector<PushClipMetrics> metrics;
    metrics.reserve(4);
    for (const char* name : names) {
        auto tree_result = trees.metadata(name, 15);
        if (tree_result.is_err()) return rs::make_err(tree_result.unwrap_err());
        PushTreeMetadata tree = std::move(tree_result.unwrap());
        std::optional<float> begin;
        std::optional<float> end;
        for (const PushTreeAttribute& attribute : tree.attributes) {
            // Native compares five-word FastStrings, whose encoding folds
            // ASCII case. Decoded bank names are uppercase; XML need not be.
            const auto key = encode(attribute.name);
            if (key == encode("HStr_Vel_B") || key == encode("LStr_Vel_B")) {
                begin = attribute.first_payload;
            } else if (key == encode("Vel_E")) {
                end = attribute.first_payload;
            }
        }
        // Native leaves missing outputs untouched. The host loader rejects
        // incomplete metadata rather than supplying arbitrary initial values.
        if (!begin) return rs::make_err(std::string(name) + ": missing begin-velocity attribute");
        if (!end) return rs::make_err(std::string(name) + ": missing Vel_E attribute");
        metrics.push_back(PushClipMetrics{tree.length, *begin, *end});
    }
    return PushAttributes::from_clips({metrics[0], metrics[1], metrics[2], metrics[3]});
}

}  // namespace

rs::Result<PushingSettings, std::string> PushingSettings::load_stock(const s3::data::Collections& data,
                                                                     const s3::data::Vfs& vfs,
                                                                     const std::string& asset_root) {
    auto banks = s3::data::AnimationBanks::load(vfs, asset_root);
    if (!banks.ok) return rs::make_err(banks.error);
    auto trees = banks.value.metadata();
    if (!trees.ok) return rs::make_err(trees.error);
    AnimationMetadataTrees source(trees.value);
    return load(data, source);
}

rs::Result<PushingSettings, std::string> PushingSettings::load_stock(const s3::data::Collections& data,
                                                                     const std::string& asset_root) {
    return load_stock(data, graph_runtime::disk::read_banks(asset_root), asset_root);
}

rs::Result<PushingSettings, std::string> PushingSettings::load(const s3::data::Collections& data,
                                                               PushTreeSource& trees) {
#define S3_PUSH_TRY(var, expr)                                  \
    auto var##_result = (expr);                                 \
    if (var##_result.is_err()) return rs::make_err(var##_result.unwrap_err()); \
    auto var = var##_result.unwrap()
#define S3_PUSH_FLOAT(var, name)                                         \
    auto var##_result = data.float_("anim_motion", "pushing", name);     \
    if (!var##_result.ok) return rs::make_err(var##_result.error);       \
    const float var = var##_result.value
    // Rust struct-literal field order: curves, the four floats, regular, mongo.
    S3_PUSH_TRY(button_time_max, curve(data, "button_time_max"));
    S3_PUSH_TRY(button_time_to_dv, curve(data, "button_time_to_dv"));
    S3_PUSH_TRY(blend_speed_over_frames, curve(data, "blend_speed_over_frames"));
    S3_PUSH_TRY(blend_acc_over_frames, curve(data, "blend_acc_over_frames"));
    // Relative to the anim_motion/pushing layout pointer:1704,1788,1840,1808.
    S3_PUSH_FLOAT(teleport_window, "pushing_usemaxpushfromteleporttime");
    S3_PUSH_FLOAT(maximum_holding_acceleration, "max_holding_acc");
    S3_PUSH_FLOAT(out_speed_weight, "dynamic_out_factor_speed_vs_acc");
    S3_PUSH_FLOAT(maximum_out_factor, "last_push_out_time");
    S3_PUSH_TRY(regular, pushing_settings::attributes(trees, {"R_PUSHLSP_HSTR_N_0_CYC1", "R_PUSHHSP_HSTR_N_0_CYC1",
                                            "R_PUSHLSP_LSTR_N_0_CYC1", "R_PUSHHSP_LSTR_N_0_CYC1"}));
    S3_PUSH_TRY(mongo, pushing_settings::attributes(trees, {"R_PUSHLSP_HSTR_MONGO_0_CYC1", "R_PUSHHSP_HSTR_MONGO_0_CYC1",
                                          "R_PUSHLSP_LSTR_MONGO_0_CYC1", "R_PUSHHSP_LSTR_MONGO_0_CYC1"}));
#undef S3_PUSH_TRY
#undef S3_PUSH_FLOAT
    PushingSettings out;
    out.curves = PushAnimationCurves{button_time_max, button_time_to_dv, blend_speed_over_frames,
                                     blend_acc_over_frames};
    out.teleport_window = teleport_window;
    out.maximum_holding_acceleration = maximum_holding_acceleration;
    out.out_speed_weight = out_speed_weight;
    out.maximum_out_factor = maximum_out_factor;
    out.regular = regular;
    out.mongo = mongo;
    return out;
}

}  // namespace s3::game::graph_host::pushing_settings
