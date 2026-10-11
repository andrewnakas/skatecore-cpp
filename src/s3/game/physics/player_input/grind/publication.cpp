// Port of skate-game/src/physics/player_input/grind/publication.rs.
#include "s3/game/physics/player_input/grind/publication.hpp"

#include "s3/game/physics/grind/family.hpp"

namespace s3::game::physics::player_input::grind::publication {

using ::s3::game::physics::grind::Family;
namespace obs = ::s3::game::physics::grind::observation;

namespace {
rs::Result<Family, std::string> family(std::uint32_t value) {
    switch (value) {
        case 0: return Family::FiftyFifty;
        case 1: return Family::Boardslide;
        case 2: return Family::Tipslide;
        case 3: return Family::FiveO;
        case 4: return Family::Backslash;
        case 5: return Family::Darkslide;
        default: return rs::make_err(fmt::cat("Invalid published grind family ", value));
    }
}
}  // namespace

void surface(GrindInvestigationFields& fields, const GrindSurface& surface) {
    fields.center_1360 = raw(surface.center);
    fields.far_points_1376_1392 = {raw(surface.far_points[0]), raw(surface.far_points[1])};
    fields.upmost_normal_1408 = raw(surface.upmost_normal);
    fields.surface_direction_1424 = raw(surface.direction);
    fields.high_side_1440 = raw(surface.high_side);
    fields.normal_limits_1456_1460 = surface.normal_limits;
    fields.geometry_kind_1464 = static_cast<std::uint32_t>(surface.kind);
    fields.audio_surface_1468 = surface.audio_surface;
    fields.physics_surface_1472 = surface.physics_surface;
    fields.geometry_flags_1476 = surface.flags;
}

rs::Result<ManagerObservation, std::string> observation(const ProcessedPhysicsInput& p, PostContext c,
                                                        std::optional<PrimitiveMetadata> metadata,
                                                        const manager::Jumper& jumper) {
    const auto& f = p.grind;
    ManagerObservation out;
    out.geometry.point_1120 = float_(f.point_1120);
    out.geometry.direction_1136 = float_(f.direction_1136);
    out.geometry.normal_1152 = float_(f.normal_1152);
    out.geometry.target_up_1168 = float_(f.target_up_1168);
    out.geometry.primitive_start_1264 = float_(f.primitive_start_1264);
    out.geometry.primitive_end_1280 = float_(f.primitive_end_1280);
    out.geometry.spline_guids_1296 = metadata ? metadata->spline_guids : std::nullopt;
    out.geometry.upmost_normal_1408 = float_(f.upmost_normal_1408);
    out.geometry.high_side_1440 = float_(f.high_side_1440);
    out.geometry.kind_1464 = f.geometry_kind_1464;
    out.geometry.flags_1476 = f.geometry_flags_1476;
    out.geometry.impact_speed_1492 = f.impact_speed_1492;
    out.surface.audio_surface_1468 = f.audio_surface_1468;
    out.surface.material_1472 = f.physics_surface_1472;
    out.surface.friction_vs_time_1496 = f.friction_1496;
    out.surface.reckon_blend_selector_1500 = f.exit_lean_1500;
    out.surface.gravity_relief_1512 = f.gravity_relief_1512;
    S3G_TRY(out.control.family, family(f.family_1248));
    out.control.flags_1516 = f.flags_1516;
    out.control.flags_2468 = p.flags_2468;
    out.control.flags_2488 = p.flags_2488;
    out.control.translation_2796 = c.translation_2796;
    out.control.balance_2800 = c.stability_nudge_2800;
    out.control.exit_lean = f.exit_lean_1500;
    out.engagement.velocity_1184 = float_(f.entry_velocity_1184);
    out.engagement.kind_1248 = f.family_1248;
    out.jumper.geometry_kind_16 = jumper.geometry.geometry_kind;
    S3G_TRY(out.jumper.family_20, family(jumper.family));
    out.jumper.energy_24 = jumper.energy;
    out.jumper.high_side_32 = jumper.geometry.high_side;
    out.jumper.normal_48 = jumper.geometry.normal;
    out.jumper.direction_64 = jumper.geometry.direction;
    out.jumper.upmost_normal_80 = jumper.geometry.upmost;
    out.jumper.point_96 = jumper.geometry.point;
    return out;
}

}  // namespace s3::game::physics::player_input::grind::publication
