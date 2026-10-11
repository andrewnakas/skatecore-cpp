// Port of skate-game/src/physics/wipeout_states/drive_settings.rs
//! Native82BE7860's per-part records and82BEB0C8/82BEB790 settings.
#include "s3/game/physics/wipeout_states/drive_settings.hpp"

#include "s3/data/physics_skeleton.hpp"

namespace s3::game::physics::wipeout_states::drive_settings {

using ::s3::player::wipeout_state::drives::Settings;

rs::Result<Settings, std::string> load(const sd::Collections& data, const sd::Vfs& vfs, const std::string& asset_root,
                                       const std::string& primary_bank_sha) {
    S3G_TRY(const sd::PhysicsSkeleton skeleton,
            sd::PhysicsSkeleton::load(vfs, sd::join_path(asset_root, "private/stock/physics-skeletons.json"),
                                      primary_bank_sha, "PHYS_TPOSE"));
    if (skeleton.bones.size() != 24) {
        return rs::make_err(std::string("Wipeout requires the actual24 physical skeleton parts"));
    }
    Settings s{};
    for (std::size_t part = 1; part < 24; ++part) {
        const std::string name = "PART_" + skeleton.bones[part].name;
        S3G_TRY(const auto words, data.words<9>("physics_skeleton_drives", "default", name));
        for (std::size_t i = 0; i < 5; ++i) s.bone[part][i] = rs::from_bits(words[i]);
    }
    const std::array<const char*, 4> roots{"root_drive_start_scalar", "root_drive_scalar",
                                           "root_drive_controlled_scalar", "root_drive_end_scalar"};
    for (std::size_t i = 0; i < 4; ++i) {
        S3G_TRY(s.root[i], data.float_("physics_skeleton_drives", "default", roots[i]));
    }
    S3G_TRY(s.strength[0], data.float_("animation", "default", "DriveStrengthLocal"));
    S3G_TRY(s.strength[1], data.float_("animation", "default", "DriveStrengthRootLocal"));
    S3G_TRY(s.hook_spring, data.float_("physics_animation", "default", "HookSoftDsp"));
    S3G_TRY(s.hook_strength, data.float_("physics_animation", "default", "HookSoftStr"));
    S3G_TRY(s.hook_damping, data.float_("physics_animation", "default", "HookSoftDmp"));
    return s;
}

}  // namespace s3::game::physics::wipeout_states::drive_settings
