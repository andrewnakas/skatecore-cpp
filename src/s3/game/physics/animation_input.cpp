// Port of skate-game/src/physics/animation_input.rs.
#include "s3/game/physics/animation_input.hpp"

#include "s3/game/difficulty.hpp"

namespace s3::game::physics::animation_input {

namespace name = sc::animation::skeleton_input::name;
namespace contact_events = sc::animation::skeleton_input::contact_events;
namespace scalar_attributes = sc::animation::skeleton_input::scalar_attributes;
namespace extended_attributes = sc::animation::skeleton_input::extended_attributes;
namespace attribute_finalization = sc::animation::skeleton_input::attribute_finalization;

namespace {
bool eq_ignore_ascii_case(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}
}  // namespace

rs::Result<AnimationInput, std::string> AnimationInput::load(const sd::Collections& data,
                                                             const sd::AnimationFrames& frames,
                                                             std::string_view mode) {
    std::optional<std::size_t> right_toe;
    for (std::size_t i = 0; i < frames.bone_names.size(); ++i) {
        if (eq_ignore_ascii_case(frames.bone_names[i], "RightToeBase")) {
            right_toe = i;
            break;
        }
    }
    if (!right_toe) return rs::make_err(std::string("Stock skeleton is missing RightToeBase"));
    AnimationInput out;
    for (std::size_t i = 0; i < difficulty::NATIVE_MODES.size(); ++i) {
        S3G_TRY(out.height_overrides[i],
                data.boolean("physics_mode", difficulty::NATIVE_MODES[i], "JumpHeightOverrideEnabled"));
    }
    out.fields = scalar_attributes::ScalarAttributeInputs::reset(0, 0);
    out.extra = extended_attributes::ExtendedAttributes::reset(0.0f);
    out.contacts = contact_events::ContactEventState{0, 0.0f};
    out.cached_jump = attribute_finalization::JumpAttributeState::new_();
    // AnimOut construction82DB1E88..1EFC initializes the null name,
    // float24=0 and clears all represented high flag bits.
    out.output.grind_name = name::encode(std::string_view(""));
    out.output.flags = 0;
    out.bone_names.clear();
    out.bone_names.reserve(frames.bone_names.size());
    for (const auto& n : frames.bone_names) out.bone_names.push_back(name::encode(std::string_view(n)));
    out.right_toe = *right_toe;
    //TU3 global constructor8289F8B4 binds292 to anim_motion
    //collectionAC260B44FA3CA0A4 (jumping), not a default profile.
    S3G_TRY(out.settings.select_jump_extremes, data.boolean("anim_motion", "jumping", "clamp_jump"));
    S3G_TRY(out.settings.low_jump_threshold, data.float_("anim_motion", "jumping", "clamp_low_inclusive"));
    S3G_TRY(out.settings.high_jump_threshold, data.float_("anim_motion", "jumping", "clamp_high_inclusive"));
    S3G_TRY(out.settings.allow_height_override,
            data.boolean("physics_mode", std::string(mode), "JumpHeightOverrideEnabled"));
    S3G_TRY(out.settings.use_prepared_controls, data.boolean("physics_jump", "default", "AdjustOnPrepare"));
    out.settings.external_impulse_active = false;
    out.settings.animation_flags = 0;
    return out;
}

Status AnimationInput::select_physics_mode(std::uint32_t mode) {
    if (static_cast<std::size_t>(mode) >= height_overrides.size())
        return rs::make_err(fmt::cat("Invalid animation physics mode ", mode));
    settings.allow_height_override = height_overrides[mode];
    return ok();
}

void AnimationInput::reset_processed() {
    fields = scalar_attributes::ScalarAttributeInputs::reset(fields.flags2468, fields.flags2488);
    extra = extended_attributes::ExtendedAttributes::reset(extra.footstep_strength);
    contacts = contact_events::ContactEventState{0, 0.0f};
}

void AnimationInput::finish_output_publication() {
    output.grind_name = name::encode(std::string_view(""));
    output.flags &= 0x003fffffu;
    extra.footstep_strength = 0.0f;
}

Status AnimationInput::process(sc::animation::Slice<const sc::animation::output::attributes::AnimationAttribute> attributes,
                               sc::animation::Slice<const sc::animation::output::NativeMatrix> hierarchy,
                               float timestep, std::uint32_t animation_flags, bool external_impulse_active,
                               sc::input::controller::ActionMap& map) {
    contact_events::ContactEventPose pose;
    pose.bone_names = sc::animation::Slice<const sc::animation::output::attributes::AttributeName>(bone_names);
    pose.hierarchy = hierarchy;
    pose.trajectory_bone = 0;
    pose.right_toe_bone = static_cast<std::int32_t>(right_toe);
    pose.timestep = timestep;
    attribute_finalization::FinalizationInput finalization = settings;
    finalization.animation_flags = animation_flags;
    finalization.external_impulse_active = external_impulse_active;
    return sc::animation::skeleton_input::process_attributes::process(attributes, pose, fields, extra, contacts,
                                                                       cached_jump, output, finalization, &map);
}

}  // namespace s3::game::physics::animation_input
