// Port of skate-game/src/tests/powerslide_playback.rs (mounted in physics.rs as powerslide_tests).
//! Raw controller -> stock graphs -> canonical packet -> SlideGround regression.
// `#[ignore = "requires private stock animation banks and collections"]`: runs on the cleanroom pack,
// skips without it.
#include <cstdio>
#include <optional>
#include <sstream>

#include "physics_offboard_test_helpers.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/data/collections.hpp"

using namespace s3test::game::offboard_playback;
using s3::animation::skeleton_input::name::encode;

namespace {

void run_side(std::int16_t side) {
    auto loaded = Playback::load("normal", false);
    if (!loaded) return;
    auto& r = *loaded;
    auto data = s3::data::Collections::load(*r.vfs, s3test::game::ASSET_ROOT);
    CHECK_MSG(data.ok, data.error);
    auto threshold = data.value.float_("anim_motion", "power_slide", "slide_speed_threshold");
    CHECK_MSG(threshold.ok, threshold.error);
    const float entry_speed = threshold.value;
    auto& physics = r.physics;
    auto& skater = r.skater;
    auto& controls = r.controls;
    auto& camera = r.camera;
    // Test phases only choose controller packets. Physics and graph state are
    // never overwritten, and the production world remains intact.
    std::optional<std::uint64_t> sequence_start;
    bool saw_push = false;
    bool saw_start = false;
    bool saw_slide = false;
    bool saw_packet = false;
    bool release_clear = false;
    for (std::uint64_t tick = 0; tick < 900; ++tick) {
        if (!sequence_start && tick >= 60 && physics.riding.motion.ground_speed > entry_speed * 1.5f &&
            skater.player_state.current() == PhysicalStateId::PhysicsGround) {
            sequence_start = tick;
        }
        const std::optional<std::uint64_t> phase =
            sequence_start ? std::optional<std::uint64_t>(tick - *sequence_start) : std::nullopt;
        std::array<std::int16_t, 2> left{0, 0};
        if (phase && *phase >= 12 && *phase <= 23) {
            left = {static_cast<std::int16_t>(side * 32767), 0};
        } else if (phase && *phase >= 36 && *phase <= 95) {
            left = {static_cast<std::int16_t>(side * 22000), 32767};
        }
        auto actions = r.sample_update(
            xbox(tick >= 12 && !sequence_start ? std::uint16_t(0x1000) : std::uint16_t(0), {0, 0}, left, {0, 0}),
            false);
        const auto advanced = r.advance(actions);
        CHECK_MSG(advanced.is_ok(), "Powerslide side=" << side << " tick=" << tick << " phase="
                                                       << (phase ? std::to_string(*phase) : "None") << ": "
                                                       << advanced.unwrap_err() << "; speed="
                                                       << physics.riding.motion.ground_speed);
        saw_push = saw_push || (skater.player_input.processed.flags_2468 & (1u << 25)) != 0;
        const auto& motion = skater.animation.motion;
        const bool starts = motion.slide_latch.start(true) || motion.slide_latch.start(false);
        if (phase && *phase >= 12 && *phase <= 23) {
            CHECK_MSG(controls.action_intents.get("RightSlideStart") == nullptr &&
                          controls.action_intents.get("LeftSlideStart") == nullptr,
                      "Pure lateral stick generated a slide start: side=" << side << " tick=" << tick);
            CHECK_MSG(!starts, "Pure lateral stick started the slide latch");
            CHECK_MSG(skater.player_state.current() != PhysicalStateId::SlideGround,
                      "Pure lateral stick entered SlideGround");
        }
        if (phase && *phase >= 36 && *phase <= 95) {
            saw_start = saw_start || starts;
            saw_slide = saw_slide || skater.player_state.current() == PhysicalStateId::SlideGround;
            bool emitted = false;
            for (const auto& attribute : motion.animation.motion_attributes) {
                if (attribute.name == encode("slide") && attribute.value != 0.0f) {
                    emitted = true;
                    break;
                }
            }
            std::optional<float> published;
            const auto entries = skater.animation.attributes.entries();
            for (std::size_t i = entries.size(); i-- > 0;) {
                const auto& attribute = entries[i];
                if (attribute.name == encode("slide") && (attribute.kind == 0 || attribute.kind == 2)) {
                    if (attribute.payload._0[0]) published = s3::rs::from_bits(*attribute.payload._0[0]);
                    break;
                }
            }
            if (emitted && published) {
                CHECK_MSG(s3::rs::to_bits(skater.animation_input.fields.slide) == s3::rs::to_bits(*published),
                          "Slide changed between canonical animation packet and Skeleton input");
                saw_packet = saw_packet || *published != 0.0f;
            }
        }
        if (phase && *phase >= 97) {
            for (const char* name : {"LeftSlideStart", "RightSlideStart", "LeftSlide", "RightSlide"}) {
                CHECK_MSG(controls.action_intents.get(name) == nullptr, "Released AG intent retained: " << name);
                CHECK_MSG(motion.animation.motion_intents.get(name) == nullptr,
                          "Released MG intent retained: " << name);
            }
            release_clear = release_clear || (!motion.is_power_sliding &&
                                              skater.player_state.current() != PhysicalStateId::SlideGround);
        }
        CHECK(camera.frame.has_value());
        CHECK_EQ(skater.pose_generation, physics.ticks);
        if (phase && *phase >= 215) break;
    }
    CHECK_MSG(sequence_start.has_value(), "Real pushing never attained stock slide speed: side="
                                              << side << ", speed=" << physics.riding.motion.ground_speed
                                              << ", threshold=" << entry_speed);
    CHECK_MSG(saw_push, "No physical push was published");
    CHECK_MSG(saw_start, "Diagonal input never started the stock slide latch: side=" << side);
    CHECK_MSG(saw_packet, "No canonical nonzero slide packet reached Skeleton: side=" << side);
    CHECK_MSG(saw_slide, "Stock graph never entered SlideGround: side=" << side);
    CHECK_MSG(release_clear, "Slide failed to exit after controller release: side=" << side);
    std::fprintf(stderr, "Powerslide side=%d: stock entry, packet, physical SlideGround and release passed in %llu ticks\n",
                 side, static_cast<unsigned long long>(physics.ticks));
}

}  // namespace

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_controller_powerslide_uses_stock_graph_and_releases_both_sides) {
    if (s3test::game::skip_stale("diagonal stick never starts the slide latch; the Rust reference trace shows the same with equivalent input (docs/PARITY.md)")) return;
    for (const std::int16_t side : {std::int16_t(-1), std::int16_t(1)}) run_side(side);
}
