// Port of skate-core/src/animation/skeleton_input/catalog.rs.
// All151 scalar-name comparison sites in82BDA0D0, in native search order.
// Raw disassembly corrects the old130-entry catalog: direct-name checks were
// omitted, Spin was misidentified, and left/right grab calls were conflated.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::animation::skeleton_input::catalog {

using output::attributes::AttributeName;

struct ScalarAttribute {
    std::string_view name;
    AttributeName encoded_name;
    /// Traceability only, never an executable/data address in the host.
    std::uint32_t comparison_site = 0;
};
S3_DERIVE_EQ(ScalarAttribute, name, encoded_name, comparison_site)

constexpr ScalarAttribute entry(std::string_view name, std::uint32_t comparison_site) {
    return ScalarAttribute{name, name::encode(name), comparison_site};
}

inline constexpr std::array<ScalarAttribute, 151> SCALAR_ATTRIBUTES = {{
    entry("SlideBoard", 0x82bdacf8u),
    entry("SlideNose", 0x82bdad10u),
    entry("SlideTail", 0x82bdad28u),
    entry("Grind5050", 0x82bdad40u),
    entry("Grind5_O", 0x82bdad58u),
    entry("GrindNose", 0x82bdad70u),
    entry("GrabWorld", 0x82bdad88u),
    entry("BoardAdjustLeft", 0x82bdadb0u),
    entry("BoardAdjustRight", 0x82bdadd4u),
    entry("BoardAdjustUp", 0x82bdadf8u),
    entry("BoardAdjustDown", 0x82bdae1cu),
    entry("GrindFacingForwards", 0x82bdae40u),
    entry("GrindFacingBackwards", 0x82bdae6cu),
    entry("NoInput", 0x82bdae98u),
    entry("ExitGrind", 0x82bdaed0u),
    entry("NewAutoPump", 0x82bdaef8u),
    entry("EnteringCoffin", 0x82bdaf24u),
    entry("InCoffin", 0x82bdaf58u),
    entry("LeavingCoffin", 0x82bdaf8cu),
    entry("PlayingLanding", 0x82bdafb4u),
    entry("Balance", 0x82bdafdcu),
    entry("Spin", 0x82bdb004u),
    entry("KickTurn", 0x82bdb02cu),
    entry("Anticipating", 0x82bdb058u),
    entry("BodySpin", 0x82bdb084u),
    entry("ManualBrake", 0x82bdb0acu),
    entry("Brake", 0x82bdb0d4u),
    entry("Turn", 0x82bdb0f8u),
    entry("IsStumbling", 0x82bdb120u),
    entry("IsWipeoutPushOffImpulse", 0x82bdb148u),
    entry("IsWipeoutPushOff", 0x82bdb170u),
    entry("IsMuteRFoot", 0x82bdb198u),
    entry("IsMuteLFoot", 0x82bdb1c0u),
    entry("IsForceDownRFoot", 0x82bdb1e8u),
    entry("IsForceDownLFoot", 0x82bdb210u),
    entry("TurnScale", 0x82bdb238u),
    entry("MagScale", 0x82bdb25cu),
    entry("AnimEndCOMX", 0x82bdb280u),
    entry("AnimEndCOMY", 0x82bdb2c0u),
    entry("AnimEndCOMZ", 0x82bdb300u),
    entry("AnimTransX", 0x82bdb340u),
    entry("AnimTransY", 0x82bdb380u),
    entry("AnimTransZ", 0x82bdb3c0u),
    entry("AnimTime", 0x82bdb400u),
    entry("AnimPhysBlendSec", 0x82bdb424u),
    entry("IsAnimInterpToEdge", 0x82bdb448u),
    entry("CadenceEndPercent", 0x82bdb470u),
    entry("OB_Traj", 0x82bdb494u),
    entry("OB_Air", 0x82bdb4bcu),
    entry("Carving", 0x82bdb4e4u),
    entry("RawTurn", 0x82bdb50cu),
    entry("HardTurn", 0x82bdb534u),
    entry("Slide", 0x82bdb55cu),
    entry("FrontFlip", 0x82bdb584u),
    entry("BackFlip", 0x82bdb5acu),
    entry("Grabbing", 0x82bdb5d4u),
    entry("WantsLeftAirGrab", 0x82bdb5fcu),
    entry("WantsRightAirGrab", 0x82bdb614u),
    entry("FlipTrick", 0x82bdb62cu),
    entry("WipeOutRecover", 0x82bdb654u),
    entry("ControlledWipeout", 0x82bdb67cu),
    entry("Revert", 0x82bdb6a4u),
    entry("RevertDir", 0x82bdb6c8u),
    entry("Wipeout", 0x82bdb6f4u),
    entry("MinJump", 0x82bdb71cu),
    entry("TrickHeight", 0x82bdb73cu),
    entry("PrepareJump", 0x82bdb764u),
    entry("jump", 0x82bdb7e8u),
    entry("DangerZone", 0x82bdb810u),
    entry("LateTrick", 0x82bdb838u),
    entry("NoDangerZone", 0x82bdb860u),
    entry("ChristAir", 0x82bdb888u),
    entry("PhysGrindTranslation", 0x82bdb8e0u),
    entry("PhysGrindStabilityNudge", 0x82bdb904u),
    entry("PhysGrindUpDown", 0x82bdb928u),
    entry("PhysGrindGrabMinHeight", 0x82bdb94cu),
    entry("PhysBodySpin", 0x82bdb970u),
    entry("world_grab_y", 0x82bdb994u),
    entry("world_grab_z", 0x82bdb9b8u),
    entry("PushContact", 0x82bdb9dcu),
    entry("NewHandPlant", 0x82bdba04u),
    entry("AnimCommittedHandPlant", 0x82bdba28u),
    entry("DisableFeetTargetIK", 0x82bdba4cu),
    entry("OB_Jump", 0x82bdba70u),
    entry("OB_StandingJump", 0x82bdba98u),
    entry("OB_Sprint", 0x82bdbac0u),
    entry("OB_Turn", 0x82bdbae8u),
    entry("OB_Mag", 0x82bdbb10u),
    entry("OB_BipedWorldX", 0x82bdbb34u),
    entry("OB_BipedWorldZ", 0x82bdbb58u),
    entry("OB_LookAtX", 0x82bdbb7cu),
    entry("OB_LookAtY", 0x82bdbba0u),
    entry("OB_ObjectMvZ", 0x82bdbbc4u),
    entry("OB_ObjectMvX", 0x82bdbbe8u),
    entry("OB_ObjectMvRot", 0x82bdbc0cu),
    entry("PlayerControlledPump", 0x82bdbc30u),
    entry("OneFootAir", 0x82bdbc58u),
    entry("WipeoutControlX", 0x82bdbc7cu),
    entry("WipeoutControlY", 0x82bdbca0u),
    entry("OB_Mounting", 0x82bdbcc4u),
    entry("OB_Unmounting", 0x82bdbce8u),
    entry("OB_Dismount", 0x82bdbd0cu),
    entry("OB_HoldBoard", 0x82bdbd30u),
    entry("OB_ReleaseBoard", 0x82bdbd58u),
    entry("OB_ThrowBoard", 0x82bdbd84u),
    entry("OB_RetrieveBoard", 0x82bdbdacu),
    entry("OB_RetrievingBoard", 0x82bdbdd4u),
    entry("OB_DroppingBoard", 0x82bdbdfcu),
    entry("BipedBoardOnGround", 0x82bdbe24u),
    entry("IsGroundMount", 0x82bdbe4cu),
    entry("FootJump", 0x82bdbe74u),
    entry("AnimSkateboard", 0x82bdbe98u),
    entry("HippyJumping", 0x82bdbebcu),
    entry("PushTricking", 0x82bdbee0u),
    entry("KickoutDismount", 0x82bdbf04u),
    entry("RunoutDismount", 0x82bdbf28u),
    entry("FixLeftFoot", 0x82bdbf4cu),
    entry("FixRightFoot", 0x82bdbf70u),
    entry("DoLandOnBoard", 0x82bdbf94u),
    entry("TransitioningOnOffBoard", 0x82bdbfb8u),
    entry("JumpAboutToTakeOff", 0x82bdbfdcu),
    entry("JumpInProgress", 0x82bdc004u),
    entry("OB_JumpX", 0x82bdc02cu),
    entry("OB_JumpZ", 0x82bdc054u),
    entry("SkaterOffBoard", 0x82bdc078u),
    entry("JumpHeightOverride", 0x82bdc0a0u),
    entry("IsDark", 0x82bdc0d0u),
    entry("GrindTrick", 0x82bdc0f8u),
    entry("Shoving", 0x82bdc120u),
    entry("ShoveDirection", 0x82bdc148u),
    entry("BipedStartAngle", 0x82bdc170u),
    entry("BipedSpinAngle", 0x82bdc1a0u),
    entry("BipedAnimTime", 0x82bdc1c4u),
    entry("InManualZone", 0x82bdc1e8u),
    entry("OB_Toggle", 0x82bdc210u),
    entry("TrickFromManual", 0x82bdc238u),
    entry("WipeoutGestureStickX", 0x82bdc260u),
    entry("WipeoutGestureStickY", 0x82bdc284u),
    entry("StrongWipeoutArms", 0x82bdc2a8u),
    entry("StrongWipeoutLegs", 0x82bdc2d0u),
    entry("UnderflipBoardContact", 0x82bdc2f8u),
    entry("AudibleFootStepStrength", 0x82bdc324u),
    entry("IsTakeDownByBoard", 0x82bdc34cu),
    entry("DropInLock", 0x82bdc378u),
    entry("DropInRelease", 0x82bdc3a4u),
    entry("OB_DropIn", 0x82bdc3d0u),
    entry("BodyAdjustZ", 0x82bdc408u),
    entry("BodyAdjustX", 0x82bdc42cu),
    entry("OneFootIntent", 0x82bdc450u),
    entry("PushSpeed", 0x82bdc478u),
    entry("FootPlanting", 0x82bdc4a0u),
}};

/// First exact five-word match,824714D0. Unknown names fall through the native
/// chain without a handler; known but unimplemented handlers must not do so.
inline const ScalarAttribute* lookup(AttributeName name) {
    for (const ScalarAttribute& entry : SCALAR_ATTRIBUTES) {
        if (entry.encoded_name == name) return &entry;
    }
    return nullptr;
}

/// kind3/status&12 event name comparisons preceding the scalar chain. The
/// fifth comparison is of the event payload, not the attribute name.
inline constexpr std::array<ScalarAttribute, 5> EVENT_COMPARISONS = {{
    entry("push_contact", 0x82bdab08u),
    entry("brake_contact", 0x82bdac40u),
    entry("right_hand_grab", 0x82bdac98u),
    entry("left_hand_grab", 0x82bdacb0u),
    entry("righthand", 0x82bdacc8u),
}};

}  // namespace s3::animation::skeleton_input::catalog
