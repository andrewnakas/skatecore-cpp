// Port of skate-game/src/physics/handplant/settings.rs
//! TU3 cached layouts55B5940075F78842 and0F38084037CB9175.
#include "s3/game/physics/handplant/settings.hpp"

namespace s3::game::physics::handplant::settings {

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data) {
    const std::string root = "Hash_55B5940075F78842";
    auto graph4 = [&](const std::string& class_, const char* name) -> rs::Result<PointGraph<4>, std::string> {
        S3G_TRY(const auto words, data.words<12>(class_, "default", name));
        const auto w = rs::from_bits(words);
        PointGraph<4> g{};
        for (std::size_t i = 0; i < 4; ++i) {
            g.x[i] = w[4 + i];
            g.y[i] = w[8 + i];
        }
        return g;
    };
    auto graph8 = [&](const char* name) -> rs::Result<PointGraph<8>, std::string> {
        S3G_TRY(const auto words, data.words<20>("physics_handplantmanager", "default", name));
        const auto w = rs::from_bits(words);
        PointGraph<8> g{};
        for (std::size_t i = 0; i < 8; ++i) {
            g.x[i] = w[4 + i];
            g.y[i] = w[12 + i];
        }
        return g;
    };
    auto f = [&](const char* name) { return data.float_("physics_handplantmanager", "default", name); };
    Settings s{};
    // Rust struct-literal field order (truck_distance first).
    S3G_TRY(s.truck_distance, data.float_("physics_grinds", "default", "DeckCenterToTruck"));
    S3G_TRY(s.window[0], graph4(root, "Hash_FF0DE8A42210E8C0"));
    S3G_TRY(s.window[1], graph4(root, "Hash_B90C78786ED04A7D"));
    S3G_TRY(s.window[2], graph4(root, "Hash_DC7947C6DD03CDAA"));
    S3G_TRY(s.window[3], graph4(root, "Hash_0ACB7AAE2662FBB2"));
    S3G_TRY(s.window[4], graph4(root, "Hash_CF7F668AB995380D"));
    S3G_TRY(s.window[5], graph4(root, "Hash_8348DA199CC87B7A"));
    S3G_TRY(s.window[6], graph4(root, "Hash_F27F622CAE33BD50"));
    S3G_TRY(s.depth, data.float_(root, "default", "Hash_3D9138A04C6D43F6"));
    S3G_TRY(s.window_drop, data.float_(root, "default", "Hash_A046D0516C0BD62B"));
    S3G_TRY(s.time_warp, graph8("Hash_F551DF3289124FB0"));
    S3G_TRY(s.out_heading, graph8("Hash_8FC9A4F9002CF9E0"));
    S3G_TRY(s.into_rotation, graph8("Hash_F61B0FB73C39311B"));
    S3G_TRY(s.out_rotation, graph8("Hash_D4EF764AF4D5F1CA"));
    S3G_TRY(s.hand_radius, graph4("physics_handplantmanager", "Hash_5494E3F9BE572D6B"));
    S3G_TRY(s.curve_half_time, f("Hash_2460D6544C3B64B7"));
    S3G_TRY(s.entry_blend, f("Hash_CCD7A82CE1BBFFF2"));
    S3G_TRY(s.hand_out, f("Hash_E0895C189D77CE16"));
    S3G_TRY(s.hand_into, f("Hash_01EB855A0ACBD84C"));
    S3G_TRY(s.hand_release, f("Hash_9C02B9E6998D1321"));
    S3G_TRY(s.minimum_speed, f("Hash_63303CA6C611DC79"));
    S3G_TRY(s.minimum_slope, f("Hash_D1622917F6DD215F"));
    S3G_TRY(s.rotation_time, f("Hash_D595A32C5F02178F"));
    S3G_TRY(s.committed_time, f("Hash_B5EC5A0941B407A4"));
    S3G_TRY(s.minimum_out_speed, f("Hash_49C91803479B9BED"));
    S3G_TRY(s.hand_approach, f("Hash_7C8B87F7435221C4"));
    S3G_TRY(const auto direction_words,
            data.words<1>("physics_handplantmanager", "default", "Hash_DDE5BFBCB6A1AFD5"));
    s.direction_frames = static_cast<std::int32_t>(direction_words[0]);
    S3G_TRY(s.apex_radius, f("Hash_A046D0516C0BD62B"));
    S3G_TRY(s.apex_angle, f("Hash_90CDD541098AB4B5"));
    S3G_TRY(s.animation[0], data.float_("anim_handplant", "default", "Hash_B9012B86EB5258FE"));
    S3G_TRY(s.animation[1], data.float_("anim_handplant", "default", "Hash_512342FB1D4A7D57"));
    S3G_TRY(s.animation[2], data.float_("anim_handplant", "default", "Hash_673F3B1BD056C39E"));
    return s;
}

}  // namespace s3::game::physics::handplant::settings
