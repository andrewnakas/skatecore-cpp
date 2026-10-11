// Port of skate-game/src/physics.rs (+ physics/simref_probe.rs).
#include "s3/game/physics.hpp"

#include <cmath>

#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/colliders.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/grind_chromosome.hpp"
#include "s3/game/physics/known_air.hpp"
#include "s3/game/physics/offboard/post_physics.hpp"
#include "s3/game/physics/render_pose.hpp"
#include "s3/game/physics/skateboard_controller.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_feedback.hpp"
#include "s3/game/physics/wipeout.hpp"
#include "s3/game/physics/wipeout_states.hpp"
#include "s3/game/skate_world.hpp"

namespace s3::game::physics {

using sc::math::Vector3;
using sc::physics::board_runtime::BoardMotion;
using sc::physics::board_runtime::BoardRuntime;
using sc::physics::drive_frames::RetailAffineTransform;
using sc::physics::phase::PhysicalOutputSnapshot;
using sc::physics::phase::PhysicsCommand;
using sc::physics::phase::PhysicsCommandBuffer;
using sc::physics::phase::PhysicsEvent;
using sc::physics::phase::PhysicsEventBuffer;
using sc::player::state::PhysicalStateId;

// ---- SimulationExchange ------------------------------------------------------------

SimulationExchange SimulationExchange::new_(std::uint64_t tick) {
    return SimulationExchange{PhysicsCommandBuffer::new_(tick), PhysicsEventBuffer::new_(tick), std::nullopt};
}

Status SimulationExchange::emit_event(std::uint64_t tick, PhysicsEvent event) {
    return events_.emit(tick, std::move(event));
}

void SimulationExchange::publish_output(PhysicalOutputSnapshot output) { physical_output = std::move(output); }

const PhysicalOutputSnapshot* SimulationExchange::output() const {
    return physical_output ? &*physical_output : nullptr;
}

const std::vector<PhysicsEvent>& SimulationExchange::events() const { return events_.events(); }

Status SimulationExchange::request_state(PhysicalStateId state) {
    const std::uint64_t tick = commands.tick();
    return commands.push(tick, PhysicsCommand{PhysicsCommand::RequestState{state}});
}

// ---- GamePhysics -------------------------------------------------------------------

void GamePhysics::set_gesture_preferences(std::optional<std::array<std::uint32_t, 4>> gestures) {
    if (gestures) {
        bool all = true;
        for (std::uint32_t v : *gestures) all = all && v < 37;
        if (!all) gestures.reset();
    }
    animation_profile.gesture_selections = gestures;
}

void GamePhysics::set_equipment_preferences(float truck, float wheel) {
    if (rs::is_finite(truck) && rs::is_finite(wheel)) {
        animation_profile.truck_tightness = rs::clamp(truck, 0.0f, 1.0f);
        animation_profile.wheel_hardness = rs::clamp(wheel, 0.0f, 1.0f);
    }
}

void GamePhysics::set_difficulty(difficulty::Difficulty difficulty) {
    // Actor publication carries this selector into the next physical packet.
    // Keep the board, active trick, equipment preferences and controller history.
    animation_profile.physics_mode = static_cast<std::uint32_t>(difficulty);
}

std::uint64_t GamePhysics::simulation_elapsed_us() const {
    const double v = double(ticks) * double(settings.step.simulation.time_step) * 1000000.0;
    if (!(v > 0.0)) return 0;
    if (v >= 18446744073709551615.0) return UINT64_MAX;
    return static_cast<std::uint64_t>(v);
}

vfs_time::Duration GamePhysics::period() const { return clock.period(); }

std::uint32_t GamePhysics::difficulty_index() const { return animation_profile.physics_mode; }

const std::vector<sc::physics::board_world::WorldTriangle>& GamePhysics::world_triangles() const {
    return world.triangles();
}

std::uint32_t GamePhysics::simref_physics_mode() const { return animation_profile.physics_mode; }

rs::Result<GamePhysics, std::string> GamePhysics::load(const sd::Vfs& vfs, const std::string& asset_root) {
    return load_with_terrain(vfs, asset_root, ground::Terrain::Flat);
}

rs::Result<GamePhysics, std::string> GamePhysics::load_with_terrain(const sd::Vfs& vfs, const std::string& asset_root,
                                                                    ground::Terrain terrain) {
    return load_with_world(vfs, asset_root, terrain, nullptr);
}

rs::Result<GamePhysics, std::string> GamePhysics::load_with_world(const sd::Vfs& vfs, const std::string& asset_root,
                                                                  ground::Terrain terrain, const sd::SkateMap* map) {
    return load_world_difficulty(vfs, asset_root, terrain, map, difficulty::Difficulty::Easy);
}

rs::Result<GamePhysics, std::string> GamePhysics::load_with_map(const sd::Vfs& vfs, const std::string& asset_root,
                                                                const sd::SkateMap* map) {
    return load_with_difficulty(vfs, asset_root, map, difficulty::Difficulty::Easy);
}

rs::Result<GamePhysics, std::string> GamePhysics::load_with_difficulty(const sd::Vfs& vfs,
                                                                       const std::string& asset_root,
                                                                       const sd::SkateMap* map,
                                                                       difficulty::Difficulty difficulty) {
    return load_world_difficulty(vfs, asset_root, ground::Terrain::Course, map, difficulty);
}

rs::Result<GamePhysics, std::string> GamePhysics::load_world_difficulty(const sd::Vfs& vfs,
                                                                        const std::string& asset_root,
                                                                        ground::Terrain terrain,
                                                                        const sd::SkateMap* map,
                                                                        difficulty::Difficulty difficulty) {
    S3G_TRY(auto data, sd::Collections::load(vfs, asset_root));
    S3G_TRY(auto settings, PhysicsSettings::load(data));
    S3G_TRY(auto animation_profile, animation_phase::AnimationProfile::load(data, difficulty::key(difficulty)));
    log::eprintln(fmt::cat("SKATE_PHYSICS_MODE ", difficulty::key(difficulty), " index=",
                           animation_profile.physics_mode));
    RetailAffineTransform spawn = RetailAffineTransform::IDENTITY;
    spawn.translation =
        Vector3::new_(0.0f, ground::HEIGHT + settings.wheel_radius - settings.authored[0].translation.y, 0.0f);
    if (map != nullptr) {
        // Package spawn is the wheel-ground anchor, in native Y-up metres.
        spawn.translation = Vector3::new_(map->spawn[0],
                                          map->spawn[1] + settings.wheel_radius - settings.authored[0].translation.y,
                                          map->spawn[2]);
        // glam Mat3::from_rotation_y(angle).to_cols_array_2d(): sin_cos = (sin, cos).
        const float sina = std::sin(map->heading);
        const float cosa = std::cos(map->heading);
        spawn.basis = sc::math::Basis3{{{{cosa, 0.0f, -sina}, {0.0f, 1.0f, 0.0f}, {sina, 0.0f, cosa}}}};
    }
    auto board = BoardRuntime::new_(settings.masses, settings.authored, spawn, settings.step.simulation,
                                    BoardMotion::Active);
    std::optional<sc::physics::board_world::BoardWorld> world;
    if (map != nullptr) {
        S3G_TRY(auto w, skate_world::collision_world(*map, settings.floor_material));
        world = std::move(w);
    } else {
        world = ground::world(terrain, settings.floor_material);
    }
    std::shared_ptr<const grind_world::StaticProvider> grind_world;
    if (map == nullptr && terrain == ground::Terrain::Course) {
        S3G_TRY(auto provider, grind_world::StaticProvider::authored(grind_world::test_rails()));
        grind_world = std::make_shared<const grind_world::StaticProvider>(std::move(provider));
    } else {
        S3G_TRY(auto provider, grind_world::StaticProvider::new_(map));
        grind_world = std::make_shared<const grind_world::StaticProvider>(std::move(provider));
    }
    if (map != nullptr) {
        log::eprintln(fmt::cat("SKATE_GRIND_READY splines=", map->rails.size(),
                               " primitives=", grind_world->primitives().size()));
    }
    auto grind_materials = grind_materials::GrindMaterials::new_(settings);
    //Both authored terrains contain static collision surfaces only, with
    //no interactable objects or assembly-bound grab splines. Do not infer
    //those identities from triangle/mesh IDs. Queries use the real registry.
    S3G_TRY(auto offboard_grab_scene, offboard::grab_scene::Registry::new_(*world, {}, {}));
    const std::uint32_t processed_flags_2468 = 0x2000;
    S3G_TRY(auto riding, RidingOutputs::load(data, board, processed_flags_2468));
    const auto [query, retention] = ground::query_settings();
    return GamePhysics{
        network::Proxies{},
        false,
        0,
        clock::SimulationClock{},
        std::move(board),
        std::move(riding),
        std::move(*world),
        std::move(grind_world),
        std::move(grind_materials),
        std::move(offboard_grab_scene),
        std::move(settings),
        std::move(animation_profile),
        query,
        retention,
        0,
        0,
        false,
        SimulationExchange::new_(0),
        processed_flags_2468,
        false,
        trainer::TrainerTuning{},
    };
}

Status GamePhysics::advance_board() {
    board.clear_forces();
    S3G_CHECK(riding.start_wheel_queries(board, world));
    S3G_CHECK(riding.finish_wheel_queries());
    const auto volumes = colliders::world_volumes(board, settings);
    const auto contacts = world.query_primitives(volumes, query, retention);
    contact_count = contacts.size();
    // The graph/ground-state consumer must supply recovered steering and
    // forces before this is playable. This tick verifies physical assembly.
    board.advance(contacts, {0.0f, 0.0f}, settings.step);
    S3G_CHECK(riding.finish_post_physics(board, board_wiping_out, processed_flags_2468,
                                         settings.step.simulation.time_step));
    ticks += 1;
    for (const auto& body : board.bodies()) {
        const auto p = body.rates.position;
        if (!rs::is_finite(p.x) || !rs::is_finite(p.y) || !rs::is_finite(p.z)) {
            failed = true;
            return rs::make_err(fmt::cat("Board solver produced a non-finite pose on tick ", ticks));
        }
    }
    return ok();
}

namespace {
template <class Bodies>
std::optional<std::size_t> first_invalid(const Bodies& bodies) {
    std::size_t i = 0;
    for (const auto& body : bodies) {
        const auto p = body.rates.position;
        const auto v = body.rates.linear_velocity;
        const auto w = body.rates.angular_velocity;
        for (float value : {p.x, p.y, p.z, v.x, v.y, v.z, w.x, w.y, w.z}) {
            if (!rs::is_finite(value)) return i;
        }
        ++i;
    }
    return std::nullopt;
}
}  // namespace

Status GamePhysics::finish_skater(SkaterRuntime& skater) {
    //Skateboard::UpdatePostPhysics82C02158 prepares the wall probe for
    //the next StartBoard, using this input phase's cached deck toolkit.
    if (!skater.player_input.toolkit.has_value()) {
        return rs::make_err(std::string("Postphysics wall probe requires the current board toolkit"));
    }
    riding.probes.prepare_wall(skater.player_input.processed, *skater.player_input.toolkit);
    S3G_CHECK(riding.finish_post_physics(board, board_wiping_out, processed_flags_2468,
                                         settings.step.simulation.time_step));
    const auto partial = skateboard_controller::partial_request(skater.skateboard_controller, riding.ground);
    skeleton_feedback::publish(*this, skater, partial);
    if (skater.player_state.current() == PhysicalStateId::WipeoutGround) {
        wipeout_states::post_physics(skater);
    }
    if (skater.player_state.current() == PhysicalStateId::PhysicsAir) {
        air_phase::update_apex(*this, skater.air_state);
    }
    if (skater.player_state.current() == PhysicalStateId::KnownAir) {
        S3G_CHECK(known_air::post_physics(*this, skater));
    }
    if (sc::player::state::is_grind(skater.player_state.current()) ||
        skater.player_state.current() == PhysicalStateId::Nonspecific) {
        S3G_CHECK(grind::post(*this, skater));
    }
    S3G_CHECK(wipeout::check_after_physics(*this, skater));
    S3G_CHECK(offboard::post_physics::advance(*this, skater));
    const auto compression = skater.skeleton_output.average_compressions(board);
    S3G_CHECK(render_pose::publish(*this, skater, compression));
    ticks += 1;
    const auto invalid_board = first_invalid(board.bodies());
    const auto invalid_skeleton = first_invalid(skater.skeleton.bodies());
    if (invalid_board.has_value() || invalid_skeleton.has_value()) {
        failed = true;
        return rs::make_err(fmt::cat("Shared skater solver produced non-finite rates on tick", ticks,
                                     "; board=", invalid_board ? std::to_string(*invalid_board) : "None",
                                     "; skeleton=", invalid_skeleton ? std::to_string(*invalid_skeleton) : "None"));
    }
    return ok();
}

}  // namespace s3::game::physics

namespace s3::game::physics::skater {

std::tuple<bool, std::string_view, std::uint32_t, float> SkaterRuntime::mod_grind() const {
    const auto& out = player_input.physical.grinds;
    const bool active = grind.active_family().has_value();
    std::string_view name;
    if (active) {
        const auto found = grind_chromosome::names::lookup(out.animation_chromosome_268);
        name = found.has_value() ? std::string_view(found->attribute) : std::string_view();
    }
    return {active, name, out.words_136_140[0], grind.last_grind_distance()};
}

}  // namespace s3::game::physics::skater
