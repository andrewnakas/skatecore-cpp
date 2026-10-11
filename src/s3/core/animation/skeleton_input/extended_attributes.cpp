// Port of skate-core/src/animation/skeleton_input/extended_attributes.rs.
#include "s3/core/animation/skeleton_input/extended_attributes.hpp"

namespace s3::animation::skeleton_input::extended_attributes {

rs::Result<bool, std::string> dispatch(std::string_view name, const ScalarFn& scalar,
                                       ScalarAttributeInputs& input, ExtendedAttributes& extra,
                                       AnimationControlOutput& output) {
    const std::string_view n = name;
    if (n == "Wipeout") {
        input.flags2468 |= 0x40000u;
    } else if (n == "jump") {
        input.flags2468 |= 0x400000u;
    } else if (n == "DangerZone") {
        input.flags2472 |= 0x8000u;
    } else if (n == "LateTrick") {
        input.flags2472 |= 0x20u;
    } else if (n == "NoDangerZone") {
        input.flags2472 |= 0x10u;
    } else if (n == "ChristAir") {
        input.flags2472 |= 0x40u;
    } else if (n == "PhysGrindTranslation") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.grind_translation = v.unwrap();
    } else if (n == "PhysGrindStabilityNudge") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.grind_stability_nudge = v.unwrap();
    } else if (n == "PhysGrindUpDown") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.grind_up_down = v.unwrap();
    } else if (n == "PhysGrindGrabMinHeight") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.grind_grab_min_height = v.unwrap();
    } else if (n == "PhysBodySpin") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.physical_body_spin = v.unwrap();
    } else if (n == "world_grab_y") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.world_grab_y = v.unwrap();
    } else if (n == "world_grab_z") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.world_grab_z = v.unwrap();
    } else if (n == "NewHandPlant") {
        extra.flags2480 |= 0x20000000u;
    } else if (n == "AnimCommittedHandPlant") {
        extra.flags2480 |= 0x10000000u;
    } else if (n == "DisableFeetTargetIK") {
        extra.flags2480 |= 0x08000000u;
    } else if (n == "OB_Jump") {
        input.flags2476 |= 0x80000u;
    } else if (n == "OB_StandingJump") {
        input.flags2476 |= 0x40000u;
    } else if (n == "OB_Sprint") {
        input.flags2484 |= 0x20000u;
    } else if (n == "OB_Turn") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.offboard_turn = -v.unwrap();
    } else if (n == "OB_Mag") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.offboard_magnitude = v.unwrap();
    } else if (n == "OB_BipedWorldX") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.biped_world_x = v.unwrap();
    } else if (n == "OB_BipedWorldZ") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.biped_world_z = v.unwrap();
    } else if (n == "OB_LookAtX") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.look_x = v.unwrap();
    } else if (n == "OB_LookAtY") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.look_y = v.unwrap();
    } else if (n == "OB_ObjectMvZ") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.object_move_z = v.unwrap();
    } else if (n == "OB_ObjectMvX") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.object_move_x = v.unwrap();
    } else if (n == "OB_ObjectMvRot") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.object_move_rotation = v.unwrap();
    } else if (n == "OneFootAir") {
        extra.flags2480 |= 0x04000000u;
    } else if (n == "WipeoutControlX") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.wipeout_control[0] = v.unwrap();
    } else if (n == "WipeoutControlY") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.wipeout_control[1] = v.unwrap();
    } else if (n == "OB_Mounting") {
        extra.flags2480 |= 0x80000u;
    } else if (n == "OB_Unmounting") {
        extra.flags2480 |= 0x40000u;
    } else if (n == "OB_Dismount") {
        extra.flags2480 |= 0x20000u;
    } else if (n == "OB_HoldBoard") {
        input.flags2476 |= 0x4000u;
    } else if (n == "OB_ReleaseBoard") {
        input.flags2476 = (input.flags2476 & ~0x3000u) | 0x2000u;
    } else if (n == "OB_ThrowBoard") {
        input.flags2476 |= 0x3000u;
    } else if (n == "OB_RetrieveBoard") {
        input.flags2476 |= 0x800u;
    } else if (n == "OB_RetrievingBoard") {
        input.flags2476 |= 0x400u;
    } else if (n == "OB_DroppingBoard") {
        input.flags2476 |= 0x200u;
    } else if (n == "BipedBoardOnGround") {
        input.flags2484 |= 1u;
    } else if (n == "IsGroundMount") {
        input.flags2488 |= 0x80000000u;
    } else if (n == "FootJump") {
        extra.flags2480 |= 0x2000u;
    } else if (n == "AnimSkateboard") {
        extra.flags2480 |= 0x4000u;
    } else if (n == "HippyJumping") {
        extra.flags2480 |= 0x1000u;
    } else if (n == "PushTricking") {
        extra.flags2480 |= 0x800u;
    } else if (n == "KickoutDismount") {
        extra.flags2480 |= 0x100u;
    } else if (n == "RunoutDismount") {
        extra.flags2480 |= 0x80u;
    } else if (n == "FixLeftFoot") {
        extra.flags2480 |= 0x40u;
    } else if (n == "FixRightFoot") {
        extra.flags2480 |= 0x20u;
    } else if (n == "DoLandOnBoard") {
        extra.flags2480 |= 0x10u;
    } else if (n == "TransitioningOnOffBoard") {
        extra.flags2480 |= 4u;
    } else if (n == "JumpAboutToTakeOff") {
        input.flags2484 |= 0x40000000u;
    } else if (n == "JumpInProgress") {
        input.flags2484 |= 0x20000000u;
    } else if (n == "OB_JumpX") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.offboard_jump[0] = -v.unwrap();
    } else if (n == "OB_JumpZ") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.offboard_jump[1] = v.unwrap();
    } else if (n == "SkaterOffBoard") {
        input.flags2484 |= 0x08000000u;
    } else if (n == "IsDark") {
        input.flags2484 |= 0x200000u;
    } else if (n == "GrindTrick") {
        input.flags2484 |= 0x100000u;
    } else if (n == "Shoving") {
        input.flags2484 |= 0x80000u;
    } else if (n == "ShoveDirection") {
        input.flags2484 |= 0x40000u;
    } else if (n == "BipedStartAngle") {
        input.flags2484 |= 0x4000u;
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.biped_start_angle = v.unwrap();
    } else if (n == "BipedSpinAngle") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.biped_spin_angle = v.unwrap();
    } else if (n == "BipedAnimTime") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.biped_animation_time = v.unwrap();
    } else if (n == "InManualZone") {
        input.flags2484 |= 0x2000u;
    } else if (n == "OB_Toggle") {
        input.flags2484 |= 0x1000u;
    } else if (n == "TrickFromManual") {
        input.flags2484 |= 0x800u;
    } else if (n == "WipeoutGestureStickX") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.wipeout_gesture[0] = v.unwrap();
    } else if (n == "WipeoutGestureStickY") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.wipeout_gesture[1] = v.unwrap();
    } else if (n == "StrongWipeoutArms") {
        input.flags2484 |= 0x20u;
    } else if (n == "StrongWipeoutLegs") {
        input.flags2484 |= 0x10u;
    } else if (n == "UnderflipBoardContact") {
        output.flags |= 0x04000000u;
    } else if (n == "AudibleFootStepStrength") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.footstep_strength = v.unwrap();
    } else if (n == "IsTakeDownByBoard") {
        output.flags |= 0x02000000u;
    } else if (n == "DropInLock") {
        output.flags |= 0x01000000u;
    } else if (n == "DropInRelease") {
        output.flags |= 0x00800000u;
    } else if (n == "OB_DropIn") {
        output.flags |= 0x00400000u;
        input.flags2488 |= 0x10000000u;
    } else if (n == "BodyAdjustZ") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.body_adjust[1] = v.unwrap();
    } else if (n == "BodyAdjustX") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        extra.body_adjust[0] = v.unwrap();
    } else if (n == "OneFootIntent") {
        input.flags2488 |= 0x02000000u;
    } else if (n == "FootPlanting") {
        extra.flags2480 |= 0x00800000u;
    } else {
        return false;
    }
    return true;
}

}  // namespace s3::animation::skeleton_input::extended_attributes
