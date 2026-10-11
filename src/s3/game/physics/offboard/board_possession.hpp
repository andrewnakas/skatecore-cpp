// Port of skate-game/src/physics/offboard/board_possession.rs
//! Live board controller adapter. The caller borrows the existing board and
//! state448 owners; this module never advances a second physics island.
#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/player/offboard/board_possession.hpp"
#include "s3/core/player/offboard/board_possession/angular.hpp"
#include "s3/core/player/offboard/board_possession/lifecycle.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::board_possession {

using sc::math::Basis3;
using sc::math::Vector3;
using sc::physics::board_runtime::BoardRuntime;
using sc::physics::contact::RetailContactMaterial;
using sc::physics::drive_frames::RetailAffineTransform;
using sc::player::offboard::board_possession::Frame;
using sc::player::offboard::board_possession::Vector;
using sc::player::offboard::board_possession::lifecycle::Alignment;
using NativeEffects = sc::player::offboard::board_possession::lifecycle::Effects;

/// Three shared shapes: deck aggregate A064, truck capsule A87C used by both
/// trucks A8B0/A8FC, wheel sphere AB94 assigned to all four wheels AC44.
/// Initialize from the actual authored shape flags, then collision generation
/// reads these live flags instead of immutable construction settings.
struct VolumeFlags {
    bool deck = false;
    bool trucks = false;
    bool wheels = false;
    std::vector<bool> deck_children;

    void set_enabled(bool enabled);
};

/// Borrow the actual three material slots read by colliders.rs. A detached
/// seven-element copy is not a live collision publication in this host.
struct Materials {
    RetailContactMaterial* wheels = nullptr;
    RetailContactMaterial* trucks = nullptr;
    RetailContactMaterial* deck = nullptr;

    /// private
    void set(const std::array<RetailContactMaterial, 3>& values);
};

struct Effects : NativeEffects {
    BoardRuntime* board = nullptr;
    std::uint8_t* animated_290 = nullptr;
    bool* wiping_out = nullptr;
    bool* alignment_active = nullptr;
    Alignment* alignment_ = nullptr;  // Rust field `alignment` (renamed: clashes with the trait method)
    VolumeFlags* volumes = nullptr;
    Materials materials{};
    /// Wheels, trucks, deck, from their original standard-material settings.
    std::array<RetailContactMaterial, 3> standard_materials{};
    RetailContactMaterial released_material{};
    ///091F8: actual8D3065CED858A7DF field multiplied by822F860C.
    float standard_angular_drag = 0.0f;
    float processed_dt_2604 = 0.0f;

    // `impl NativeEffects for Effects`.
    void enable_animation_soft() override;
    void enable_animation_angular_only() override;
    void disable_animation() override;
    void disable_linear_drive() override;
    void standard_board() override;
    void released_board() override;
    void collision_volumes(bool enabled) override;
    void clear_alignment() override;
    void alignment(Alignment value) override;
    void velocity(Vector value) override;
    void position(Vector value) override;
    void hook_frame(Frame value) override;
    void target_position_velocity(Vector value) override;
    void torque(Vector value) override;
};

Vector3 xyz(Vector v);
Vector vector(Vector3 v);
RetailAffineTransform affine(const Frame& v);

}  // namespace s3::game::physics::offboard::board_possession

#include "s3/game/physics/offboard/board_possession/drives.hpp"
#include "s3/game/physics/offboard/board_possession/settings.hpp"
