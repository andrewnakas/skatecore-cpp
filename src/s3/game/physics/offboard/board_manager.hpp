// Port of skate-game/src/physics/offboard/board_manager.rs
//! Persistent SkateboardController adapter, not the grab-spline query manager.
//!
//! TU3 82D74FD8 creates two hand/deck drives; 82DB6150 updates possession
//! after the physical state's Update and before the common board update.
//! State448/word444/system452 are borrowed from the existing controller. Never
//! create another copy in SkaterRuntime, or infer ownership from graph flags.
#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/drive_solver.hpp"
#include "s3/core/player/lifecycle.hpp"
#include "s3/core/player/offboard/board_possession.hpp"
#include "s3/core/player/offboard/board_possession/lifecycle.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/offboard/board_possession.hpp"

namespace s3::game::physics::offboard::board_manager {

namespace native = ::s3::player::offboard::board_possession;
using board_possession::Effects;
using native::Frame;
using native::lifecycle::Observation;
using native::lifecycle::Settings;
using sc::physics::assembly::BodySnapshot;
using sc::physics::drive_solver::RetailDriveRows;
using sc::player::lifecycle::SkateboardControllerActions;
using sc::player::lifecycle::SkateboardControllerFields;

struct Owner {
    native::State state{};
    // Private in Rust.
    Settings settings{};

    static rs::Result<Owner, std::string> load(const sd::Collections& data);

    /// Call once after the selected physical state Update. Observations must
    /// come from the evaluated skeleton and completed contact publications.
    void update(SkateboardControllerFields& fields, const Observation& observation, Effects& effects);
    void hold(SkateboardControllerFields& fields, const Observation& observation, Effects& effects);
    /// LetGo does not assign state448: the physical-state lifecycle or
    /// UpdateState caller does that after disabling the physical drives.
    void let_go(SkateboardControllerFields& fields, const Observation& observation, Effects& effects);
    void stop(SkateboardControllerFields& fields, const Observation& observation, Effects& effects);
    ///82DB8CE4..8D1C and82DB8D6C..8DA4: clear countdown even when
    ///already off, Stop, then74D30 resets retrieval only (not hand frames).
    void reset_for_teleport(SkateboardControllerFields& fields, const Observation& observation, Effects& effects);
    /// Append to the existing island's drive rows, with the SAME reaction
    /// indices used for skeleton part3/7 and deck6 by the contact/joint passes.
    /// There is no separate board simulation and no render-bone attachment.
    void append_drives(BodySnapshot deck, std::array<BodySnapshot, 2> hands, std::size_t deck_reaction,
                       std::array<std::size_t, 2> hand_reactions, float dt,
                       std::vector<RetailDriveRows>& rows) const;
    /// 82D76D20: pass the real Skeleton::GetBoneTransform(11), not a hand,
    /// root, deck frame or bind-pose substitute.
    native::Fill fill(const SkateboardControllerFields& fields, const native::Processed& processed,
                      Frame bone_11) const;
};

/// SetPhysicsState owns a mutable StateChangeData while invoking callbacks.
/// This short-lived adapter borrows the real physical owners, and retains only
/// the pre-transition controller fields needed by LetGo (notably RETURNING).
/// It executes effects immediately; it is not a deferred event queue.
struct Transition : SkateboardControllerActions {
    Owner* owner = nullptr;
    const Observation* observation = nullptr;
    Effects* effects = nullptr;
    // Private in Rust.
    SkateboardControllerFields previous{};
    bool changed = false;

    Transition(Owner& owner_, const Observation& observation_, Effects& effects_,
               SkateboardControllerFields previous_)
        : owner(&owner_), observation(&observation_), effects(&effects_), previous(previous_) {}
    static Transition new_(Owner& owner, const Observation& observation, Effects& effects,
                           SkateboardControllerFields previous) {
        return Transition(owner, observation, effects, previous);
    }
    /// Merge only the throw countdown. The caller's state448/system452 writes
    /// must survive. LetGo sets word444=8 for the TU3 throw bit; discarding it
    /// would lose the subsequent eight UpdateFreeBoard torque applications.
    void finish(SkateboardControllerFields& fields) &&;

    /// `impl SkateboardControllerActions for Transition`.
    void hold_skateboard() override;
    void let_go_of_skateboard() override;
};

}  // namespace s3::game::physics::offboard::board_manager

#include "s3/game/physics/offboard/board_manager/runtime.hpp"
