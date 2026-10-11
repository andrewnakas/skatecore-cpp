// Port of skate-game/src/graph_host/tests/pushing.rs.
#include <cstdio>

#include "harness.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/graph_host/pushing.hpp"
#include "s3/game/graph_host/pushing_settings.hpp"

using namespace s3::game::graph_host::pushing;
using s3::data::Collections;
using s3::data::GraphAttribute;
using s3::game::graph_host::pushing_settings::PushTreeAttribute;
using s3::game::graph_host::pushing_settings::PushTreeMetadata;
using s3::game::graph_host::pushing_settings::PushTreeSource;
using s3::point_graph::PointGraph;
using s3::riding::push_animation::PushAnimationCurves;
using s3::riding::push_animation::PushBlendParameters;
using s3::riding::push_behaviors::PushAttributes;
using s3::riding::push_behaviors::PushClipMetrics;

namespace {

PushingSettings settings() {
    PointGraph<8> curve;
    curve.x = s3::rs::from_fn<8>([](std::size_t i) { return static_cast<float>(i); });
    curve.y.fill(1.0f);
    const PushAttributes regular = PushAttributes::from_clips({
        PushClipMetrics{1.0f, 0.0f, 4.0f},
        PushClipMetrics{2.0f, 4.0f, 8.0f},
        PushClipMetrics{1.0f, 0.0f, 1.0f},
        PushClipMetrics{2.0f, 4.0f, 5.0f},
    });
    const PushAttributes mongo = PushAttributes::from_clips(s3::rs::map(regular.clips, [](PushClipMetrics c) {
        c.begin_velocity += 2.0f;
        c.end_velocity += 5.0f;
        return c;
    }));
    PushingSettings s;
    s.curves = PushAnimationCurves{curve, curve, curve, curve};
    s.teleport_window = 0.5f;
    s.maximum_holding_acceleration = 4.0f;
    s.out_speed_weight = 0.4f;
    s.maximum_out_factor = 0.659f;
    s.regular = regular;
    s.mongo = mongo;
    return s;
}

PushState state() {
    const PushBlendParameters unset{-1.0f, -1.0f, -1.0f};
    PushState s;
    s.out_factor = 0.375f;
    s.current_push_dv = 2.5f;
    s.current = unset;
    s.target = unset;
    s.continue_push = false;
    return s;
}

PushContext context(const PushingSettings& settings, PushState& state, const IntentMap& intents) {
    return PushContext{settings, state, intents, 2.0f, 1.0f, 1.0f, false, std::nullopt};
}

std::vector<std::string_view> names(const std::vector<SettableAttribute>& sink) {
    std::vector<std::string_view> out;
    for (const auto& a : sink) out.push_back(a.name);
    return out;
}

}  // namespace

S3_TEST(authored_defaults_match_tu3_factories) {
    auto attributes = [](const char* name) {
        GraphAttribute a;
        a.name = "name";
        a.text = name;
        a.boolean_byte = 0;
        a.float_bits = 0;
        return std::vector<GraphAttribute>{a};
    };
    const std::pair<const char*, PushOperation> cases[] = {
        {"PushCycle", PushOperation(PushOperation::PushCycle{"LeftPush"})},
        {"PushOut", PushOperation(PushOperation::PushOut{true})},
        {"SetPushCoefs", PushOperation(PushOperation::SetPushCoefs{false})},
        {"ComputeRepushDeadline", PushOperation(PushOperation::ComputeRepushDeadline{true})},
    };
    for (const auto& [name, expected] : cases) {
        const auto attrs = attributes(name);
        CHECK(PushOperation::from_attributes(Attributes(attrs)) == std::optional<PushOperation>(expected));
    }
}

S3_TEST(first_update_only_blends_once_per_activation_and_publishes_in_source_order) {
    const PushingSettings settings_ = settings();
    PushState state_ = state();
    const IntentMap intents;
    PushContext ctx = context(settings_, state_, intents);
    ctx.shared.target = PushBlendParameters{0.25f, 0.5f, 0.75f};
    PushInstance node = PushInstance::new_(PushOperation::SetPushCoefs{true});
    std::vector<SettableAttribute> sink;
    node.begin(ctx, sink);
    CHECK(sink.empty());
    node.update(ctx, sink).unwrap();
    CHECK(names(sink) == (std::vector<std::string_view>{"HStr_Vel_B", "LStr_Vel_B", "Vel_E"}));
    for (const auto& a : sink) CHECK(a.normalized && a.sequence_id == -1);
    const std::vector<SettableAttribute> first = sink;
    ctx.shared.target.vel_e = 0.1f;
    node.update(ctx, sink).unwrap();
    CHECK(sink == first);
    node.begin(ctx, sink);
    node.update(ctx, sink).unwrap();
    CHECK_EQ(sink[2].value, 0.1f);
}

S3_TEST(target_and_out_factor_select_attributes_using_switch_state) {
    const PushingSettings settings_ = settings();
    PushState state_ = state();
    const IntentMap intents;
    PushContext ctx = context(settings_, state_, intents);
    std::vector<SettableAttribute> sink;
    PushInstance target = PushInstance::new_(PushOperation::ComputeTargetCoefsFromSpeedAndStrength{true});
    target.update(ctx, sink).unwrap();
    CHECK(ctx.shared.target == settings_.regular.target(2.0f, 2.5f));
    ctx.is_switch = true;
    target.update(ctx, sink).unwrap();
    CHECK(ctx.shared.target == settings_.mongo.target(2.0f, 2.5f));
    CHECK(settings_.regular.target(2.0f, 2.5f) != settings_.mongo.target(2.0f, 2.5f));
    PushInstance out = PushInstance::new_(PushOperation::ComputeRepushDeadline{true});
    out.begin(ctx, sink);
    out.update(ctx, sink).unwrap();
    CHECK_EQ(ctx.shared.out_factor, 0.375f);
    out.end(ctx);
    CHECK_EQ(ctx.shared.out_factor, settings_.mongo.out_factor(2.0f, 2.5f, 0.4f, 0.659f));
    CHECK(sink.empty());
}

S3_TEST(push_out_projects_actual_selected_foot_and_publishes_only_on_begin) {
    const PushingSettings settings_ = settings();
    PushState state_ = state();
    const IntentMap intents;
    PushContext ctx = context(settings_, state_, intents);
    PushFootFrame frame;
    frame.left_foot = {2.0f, 3.0f, 4.0f, 0.0f};
    frame.right_foot = {5.0f, 6.0f, 7.0f, 0.0f};
    frame.deck_position = {1.0f, 1.0f, 1.0f, 0.0f};
    frame.deck_y = {0.0f, 1.0f, 0.0f, 0.0f};
    frame.deck_z = {0.0f, 0.0f, 1.0f, 0.0f};
    frame.skateboard_flipped = true;
    ctx.foot_frame = frame;
    PushInstance node = PushInstance::new_(PushOperation::PushOut{true});
    std::vector<SettableAttribute> sink;
    node.begin(ctx, sink);
    CHECK(sink == (std::vector<SettableAttribute>{
                      SettableAttribute{"OutDistanceX", -6.0f, false, -1},
                      SettableAttribute{"OutDistanceY", 5.0f, false, -1},
                  }));
    ctx.foot_frame = std::nullopt;
    node.update(ctx, sink).unwrap();
    node.end(ctx);
    CHECK_EQ(sink[0].value, -6.0f);
}

S3_TEST(settable_sink_updates_first_matching_name_and_sequence_without_reordering) {
    std::vector<SettableAttribute> sink = {
        SettableAttribute{"Vel_E", 0.1f, true, 7},
        SettableAttribute{"Vel_E", 0.2f, true, -1},
        SettableAttribute{"Vel_E", 0.3f, true, -1},
    };
    set_attribute(sink, SettableAttribute{"Vel_E", 3.0f, false, -1});
    std::vector<float> values;
    for (const auto& a : sink) values.push_back(a.value);
    CHECK(values == (std::vector<float>{0.1f, 3.0f, 0.3f}));
    CHECK(!sink[1].normalized);
}

namespace {

struct Trees : PushTreeSource {
    std::vector<std::string> requested;
    bool missing_end = false;

    s3::rs::Result<PushTreeMetadata, std::string> metadata(std::string_view name, std::uint32_t mask) override {
        CHECK_EQ(mask, 15u);
        requested.emplace_back(name);
        std::vector<PushTreeAttribute> attributes = {
            PushTreeAttribute{"HStr_Vel_B", 1.0f},
            PushTreeAttribute{"Unrelated", 50.0f},
            PushTreeAttribute{"LStr_Vel_B", 2.0f},
        };
        if (!missing_end) attributes.push_back(PushTreeAttribute{"Vel_E", 6.0f});
        return PushTreeMetadata{0.5f, attributes};
    }
};

Collections collections() {
    std::vector<std::string> fields;
    for (const char* name : {"pushing_usemaxpushfromteleporttime", "max_holding_acc",
                             "dynamic_out_factor_speed_vs_acc", "last_push_out_time"}) {
        fields.push_back(std::string("\"") + name + "\":{\"type\":\"EA::Reflection::Float\",\"data\":\"3F000000\"}");
    }
    std::string data;
    for (int i = 0; i < 20; ++i) {
        char word[16];
        std::snprintf(word, sizeof word, "%08X", s3::rs::to_bits(static_cast<float>(i)));
        data += word;
    }
    for (const char* name : {"button_time_max", "button_time_to_dv", "blend_speed_over_frames",
                             "blend_acc_over_frames"}) {
        fields.push_back(std::string("\"") + name + "\":{\"type\":\"Graph\",\"data\":\"" + data + "\"}");
    }
    std::string joined;
    for (std::size_t i = 0; i < fields.size(); ++i) joined += (i ? "," : "") + fields[i];
    const std::string json =
        "{\"version\":1,\"collections\":[{\"class\":\"anim_motion\",\"key\":\"pushing\",\"parent\":\"\","
        "\"fields\":{" +
        joined + "},\"source\":\"test\",\"sha256\":\"test\"}]}";
    // Rust writes this to a temporary directory and loads it; the C++ loader reads a Vfs,
    // so the same file is installed at the same relative path in an in-memory tree.
    const std::string root = "skate-pushing-test";
    s3::data::Vfs vfs;
    vfs.add_file(root + "/private/stock/skater-collections.json", std::vector<std::uint8_t>(json.begin(), json.end()));
    auto result = Collections::load(vfs, root);
    if (!result.ok) s3test::fail(__FILE__, __LINE__, result.error);
    return result.value;
}

}  // namespace

S3_TEST(metadata_loader_uses_all_eight_authored_names_and_last_matching_attribute) {
    Trees trees;
    trees.missing_end = false;
    const PushingSettings s = PushingSettings::load(collections(), trees).unwrap();
    CHECK(trees.requested == (std::vector<std::string>{
                                 "R_PUSHLSP_HSTR_N_0_CYC1", "R_PUSHHSP_HSTR_N_0_CYC1", "R_PUSHLSP_LSTR_N_0_CYC1",
                                 "R_PUSHHSP_LSTR_N_0_CYC1", "R_PUSHLSP_HSTR_MONGO_0_CYC1",
                                 "R_PUSHHSP_HSTR_MONGO_0_CYC1", "R_PUSHLSP_LSTR_MONGO_0_CYC1",
                                 "R_PUSHHSP_LSTR_MONGO_0_CYC1"}));
    CHECK_EQ(s.regular.clips[0].begin_velocity, 2.0f);
    CHECK_EQ(s.regular.clips[0].end_velocity, 6.0f);
    CHECK(s.curves.button_time_max.x == (std::array<float, 8>{4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f}));
    CHECK(s.curves.button_time_max.y ==
          (std::array<float, 8>{12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f}));
}

S3_TEST(missing_clip_metadata_is_reported_without_guessed_velocity) {
    Trees trees;
    trees.missing_end = true;
    const std::string error = PushingSettings::load(collections(), trees).unwrap_err();
    CHECK(error.find("R_PUSHLSP_HSTR_N_0_CYC1: missing Vel_E") != std::string::npos);
}
