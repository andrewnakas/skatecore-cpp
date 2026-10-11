// Port of skate-game/src/physics.rs.
//! Host game physics ownership and schedule. Physical calculations stay in core.
#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/phase.hpp"
#include "s3/core/player/state.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"
#include "s3/game/difficulty.hpp"
#include "s3/game/grind_world.hpp"
#include "s3/game/physics/animation_phase.hpp"
#include "s3/game/physics/clock.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/grind_materials.hpp"
#include "s3/game/physics/ground.hpp"
#include "s3/game/physics/network.hpp"
#include "s3/game/physics/offboard/grab_scene.hpp"
#include "s3/game/physics/riding_outputs.hpp"
#include "s3/game/physics/settings.hpp"
#include "s3/game/trainer.hpp"
#include "s3/game/vfs_time.hpp"

namespace s3::game::physics {

using riding_outputs::RidingOutputs;

/// Cross-phase records for the current fixed tick. Subsystems retain their
/// private native-shaped storage; only these buffers cross the coordinator.
struct SimulationExchange {
    sc::physics::phase::PhysicsCommandBuffer commands;
    sc::physics::phase::PhysicsEventBuffer events_;
    std::optional<sc::physics::phase::PhysicalOutputSnapshot> physical_output;

    static SimulationExchange new_(std::uint64_t tick);
    Status emit_event(std::uint64_t tick, sc::physics::phase::PhysicsEvent event);
    void publish_output(sc::physics::phase::PhysicalOutputSnapshot output);
    /// `Option<&PhysicalOutputSnapshot>`.
    const sc::physics::phase::PhysicalOutputSnapshot* output() const;
    const std::vector<sc::physics::phase::PhysicsEvent>& events() const;
    Status request_state(sc::player::state::PhysicalStateId state);
};

struct GamePhysics {
    network::Proxies network_proxies;
    bool network_active = false;
    std::size_t network_contacts = 0;
    clock::SimulationClock clock;
    sc::physics::board_runtime::BoardRuntime board;
    RidingOutputs riding;
    sc::physics::board_world::BoardWorld world;
    std::shared_ptr<const grind_world::StaticProvider> grind_world;
    grind_materials::GrindMaterials grind_materials;
    offboard::grab_scene::Registry offboard_grab_scene;
    PhysicsSettings settings;
    animation_phase::AnimationProfile animation_profile;
    sc::physics::collision::WorldContactSettings query;
    sc::physics::board_world::ContactRetentionSettings retention;
    std::uint64_t ticks = 0;
    std::size_t contact_count = 0;
    bool failed = false;
    SimulationExchange exchange;
    /// ProcessedPhysIn reset82BF9EF0 sets0x2000; initial stancebit20 is clear.
    /// Animation packet publication owns subsequent stance-bit updates.
    std::uint32_t processed_flags_2468 = 0;
    /// Toolkit ctor82C0680C clears8384bit7; wipeout entry/exit owns changes.
    bool board_wiping_out = false;
    trainer::TrainerTuning trainer{};

    void set_gesture_preferences(std::optional<std::array<std::uint32_t, 4>> gestures);
    void set_equipment_preferences(float truck, float wheel);
    void set_difficulty(difficulty::Difficulty difficulty);
    std::uint64_t simulation_elapsed_us() const;
    vfs_time::Duration period() const;
    std::uint32_t difficulty_index() const;
    const std::vector<sc::physics::board_world::WorldTriangle>& world_triangles() const;
    const sc::physics::board_world::BoardWorld& world_() const { return world; }

    /// Flat-world convenience used by private-asset integration tests (`#[cfg(test)] load`).
    static rs::Result<GamePhysics, std::string> load(const sd::Vfs& vfs, const std::string& asset_root);
    static rs::Result<GamePhysics, std::string> load_with_terrain(const sd::Vfs& vfs, const std::string& asset_root,
                                                                  ground::Terrain terrain);
    static rs::Result<GamePhysics, std::string> load_with_world(const sd::Vfs& vfs, const std::string& asset_root,
                                                                ground::Terrain terrain, const sd::SkateMap* map);
    static rs::Result<GamePhysics, std::string> load_with_map(const sd::Vfs& vfs, const std::string& asset_root,
                                                              const sd::SkateMap* map);
    static rs::Result<GamePhysics, std::string> load_with_difficulty(const sd::Vfs& vfs,
                                                                     const std::string& asset_root,
                                                                     const sd::SkateMap* map,
                                                                     difficulty::Difficulty difficulty);
    static rs::Result<GamePhysics, std::string> load_world_difficulty(const sd::Vfs& vfs,
                                                                      const std::string& asset_root,
                                                                      ground::Terrain terrain,
                                                                      const sd::SkateMap* map,
                                                                      difficulty::Difficulty difficulty);
    /// `#[cfg(test)] advance_board`.
    Status advance_board();
    Status finish_skater(SkaterRuntime& skater);

    // physics/simref_probe.rs (SIMREF: read-only accessors for the trace driver).
    const std::vector<sc::physics::phase::PhysicsEvent>& simref_events() const { return exchange.events(); }
    const sc::physics::phase::PhysicalOutputSnapshot* simref_output() const { return exchange.output(); }
    float simref_time_step() const { return settings.step.simulation.time_step; }
    std::uint32_t simref_physics_mode() const;
};

}  // namespace s3::game::physics
