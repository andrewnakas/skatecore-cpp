// Port of skate-game/src/physics/settings.rs.
#include "s3/game/physics/settings.hpp"

namespace s3::game::physics::settings {

using sc::math::Vector3;
using namespace sc::physics::drive_frames;
using namespace sc::physics::drive_parameters;
using namespace sc::physics::mass;
using sc::physics::board_step::BoardStepSettings;
using sc::physics::contact::RetailContactMaterial;
using sc::physics::rigid_body::RetailSimulationStep;

rs::Result<PhysicsSettings, std::string> PhysicsSettings::load(const sd::Collections& data) {
    auto f = [&](const char* cls, const char* field) { return data.float_(cls, "default", field); };
    auto b = [&](const char* cls, const char* field) { return data.boolean(cls, "default", field); };
    auto material = [&](const char* cls, const char* s, const char* d,
                        const char* r) -> rs::Result<RetailContactMaterial, std::string> {
        RetailContactMaterial m{};
        S3G_TRY(m.static_friction, f(cls, s));
        S3G_TRY(m.dynamic_friction, f(cls, d));
        S3G_TRY(m.restitution, f(cls, r));
        return m;
    };
    S3G_TRY(const float mass_factor, f("physics_world", "SkateboardMassFactor"));
    S3G_TRY(const float wheel_radius, f("physicswheels", "WheelRadius"));
    S3G_TRY(const float wheel_x_distance, f("physicswheels", "WheelXDist"));
    AuthoredTransformInputs geometry{};
    S3G_TRY(geometry.deck_mid_length, f("physicsdeck", "DeckMidLength"));
    geometry.wheel_x_distance = wheel_x_distance;
    S3G_TRY(geometry.truck_z_position_front, f("physicstrucks", "TruckZPosFront"));
    S3G_TRY(geometry.truck_z_position_back, f("physicstrucks", "TruckZPosBack"));
    S3G_TRY(geometry.truck_y_position, f("physicstrucks", "TruckYPos"));
    WheelMassSettings wheel_settings{};
    wheel_settings.radius = wheel_radius;
    S3G_TRY(wheel_settings.mass, f("physicswheels", "WheelMass"));
    wheel_settings.mass_factor = mass_factor;
    const auto wheel = wheel_mass_properties(wheel_settings);
    TruckMassSettings truck_settings{};
    truck_settings.wheel_radius = wheel_radius;
    truck_settings.wheel_x_distance = wheel_x_distance;
    truck_settings.mass_factor = mass_factor;
    S3G_TRY(truck_settings.radius_scalar, f("physicstrucks", "RadiusScalar"));
    S3G_TRY(truck_settings.half_height_scalar, f("physicstrucks", "HalfHeightScalar"));
    S3G_TRY(truck_settings.mass, f("physicstrucks", "TruckMass"));
    const auto truck = truck_mass_properties(truck_settings);
    S3G_TRY(const auto gravity_words, (data.words<4>("physics", "default", "WorldGravity")));
    const auto gravity = rs::from_bits(gravity_words);
    S3G_TRY(const float freezing, f("physics", "FreezingEnergy"));
    const auto simulation = RetailSimulationStep::fixed_60_hz(
        0,  // Our host keeps its single board active; sleeping is not scheduled.
        freezing, Vector3::new_(gravity[0], gravity[1], gravity[2]));
    DeckGeometrySettings deck_settings{};
    S3G_TRY(deck_settings.width, f("physicsdeck", "DeckWidth"));
    deck_settings.mid_length = geometry.deck_mid_length;
    S3G_TRY(deck_settings.thickness, f("physicsdeck", "DeckThickness"));
    S3G_TRY(deck_settings.back_end_size, f("physicsdeck", "DeckBackEndSize"));
    S3G_TRY(deck_settings.front_end_angle_degrees, f("physicsdeck", "DeckFrontEndAngle"));
    S3G_TRY(deck_settings.back_end_angle_degrees, f("physicsdeck", "DeckBackEndAngle"));
    S3G_TRY(const std::uint32_t capsules, data.integer("physicsdeck", "default", "DeckEndCapsules"));
    deck_settings.end_capsule_count = static_cast<std::int32_t>(capsules);
    S3G_TRY(deck_settings.enable_deck_volume_collisions, b("physicsdeck", "DeckEnableDeckVolumeCollisions"));
    S3G_TRY(deck_settings.enable_end_volume_collisions, b("physicsdeck", "DeckEnableEndVolumeCollisions"));
    const auto deck_geometry = DeckGeometry::new_(deck_settings);
    S3G_TRY(const float deck_mass, f("physicsdeck", "DeckMass"));
    S3G_TRY(const float deck_drag, f("physicsdeck", "DeckAngularDrag"));
    const auto deck = deck_mass_properties(deck_geometry, deck_mass * mass_factor, deck_drag * simulation.frequency);
    BoardStepSettings step{};
    step.simulation = simulation;
    S3G_TRY(step.iterations, data.integer("physics", "default", "RWMaxIterations"));
    RetailTruckTransformInputs truck_inputs{};
    truck_inputs.deck_mid_length = geometry.deck_mid_length;
    truck_inputs.truck_z_position_front = geometry.truck_z_position_front;
    truck_inputs.truck_z_position_back = geometry.truck_z_position_back;
    truck_inputs.truck_y_position = geometry.truck_y_position;
    S3G_TRY(truck_inputs.truck_rotation_axis_angle_degrees, f("physicstrucks", "TruckRotationAxisAngle"));
    step.base_truck_transforms = calculate_truck_transforms(truck_inputs);
    RetailTruckDriveSettings drives{};
    S3G_TRY(drives.use_linear_drives, b("physicstrucks", "UseTruckDrives"));
    S3G_TRY(drives.use_hard_linear_drives, b("physicsdeck", "UseHardDrives"));
    S3G_TRY(drives.angular_displacement, f("physicstrucks_drives", "Angular_Hard_Displacement"));
    S3G_TRY(drives.angular_damping, f("physicstrucks_drives", "Angular_Hard_Damping"));
    S3G_TRY(drives.angular_strength, f("physicstrucks_drives", "Angular_Hard_Strength"));
    step.truck_dynamics = retail_truck_drive_dynamics(drives);
    S3G_TRY(step.force_point_y_offset, f("physicsdeck", "DeckForceYOffset"));
    if (wheel_radius <= 0.0f || mass_factor <= 0.0f || step.iterations == 0) {
        return rs::make_err(std::string("Invalid stock board radius, mass factor or solver iterations"));
    }
    PhysicsSettings out{};
    out.step = step;
    out.masses = {wheel, wheel, wheel, wheel, truck, truck, deck};
    out.authored = authored_body_transforms(geometry);
    out.deck_geometry = deck_geometry;
    out.truck_shape = truck_mass_input(truck_settings).shape;
    S3G_TRY(out.truck_collisions, b("physicstrucks", "TruckEnableVolumeCollisions"));
    S3G_TRY(out.truck_material,
            material("physicstrucks", "TruckStaticFriction", "TruckDynamicFriction", "TruckRestitution"));
    S3G_TRY(out.deck_material, material("physicsdeck", "DeckStaticFriction", "DeckDynamicFriction", "DeckRestitution"));
    out.wheel_radius = wheel_radius;
    S3G_TRY(out.wheel_material,
            material("physicswheels", "WheelStaticFriction", "WheelDynamicFriction", "WheelRestitution"));
    S3G_TRY(out.standard_wheel_material,
            material("physicswheels", "WheelStaticFriction", "WheelDynamicFriction", "WheelRestitution"));
    // Ground job8277C5D8 uses context83034F34/38/3C. The
    // max/max/min combine preserves the moving volume material.
    out.floor_material = RetailContactMaterial{};
    out.floor_material.static_friction = 0.0f;
    out.floor_material.dynamic_friction = 0.0f;
    out.floor_material.restitution = 1.0f;
    S3G_TRY(out.input_magnitude_threshold, f("inputlistener", "StickMagnitudeMinToCountHeld"));
    return out;
}

}  // namespace s3::game::physics::settings
