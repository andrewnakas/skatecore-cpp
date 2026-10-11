// Port of skate-game/src/graph_host/motion_sliding_execute.rs (module `motion::slide_execution`).
#include "s3/game/graph_host/motion/slide_execution.hpp"

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion/execution.hpp"

namespace s3::game::graph_host::motion {

rs::Result<rs::Unit, std::string> MotionHost::execute_slide(BehaviorId behavior, motion_sliding::Operation operation,
                                                            const Frame& frame, std::uint8_t phase) {
    using s3::animation::playback_parameters::SettableAttribute;
    using s3::animation::skeleton_input::name::encode;
    using Operation = motion_sliding::Operation;
    if (std::holds_alternative<Operation::Update>(operation.value) && phase == 1) {
        if (!condition_inputs.physical_state) {
            return rs::make_err(std::string("PowerSliding requires actual filtered physical category"));
        }
        const std::uint32_t category = condition_inputs.physical_state->category;
        // Native returns before speed/direction reads outside category1.
        float speed = 0.0f;
        float direction = 0.0f;
        if (category == 1) {
            if (!fakie_physical) return rs::make_err(std::string("PowerSliding requires actual Motion164 speed"));
            speed = fakie_physical->ground_projected_speed;
            S3_MHOST_TRY(direction, slide_direction());
        }
        auto* state = instances.at(behavior).get<Instance::Sliding>();
        if (state == nullptr) return rs::make_err(std::string("PowerSliding has an invalid instance"));
        state->_0.update(animation, category, speed, direction, frame.dt, sliding, slide_latch);
    } else if (const auto* create = std::get_if<Operation::Create>(&operation.value)) {
        switch (phase) {
            case 0: {
                //ISkaterAnim virtual12=82B970D8 reads the live bit29.
                S3_MHOST_SOME(const std::uint32_t animation_flags, animation.skater_animation_flags,
                               "CreateSlide requires actual animation stance flags");
                const bool fakie = (animation_flags & 0x20000000u) != 0;
                slide_latch.begin_slide(fakie);
                break;
            }
            case 1: {
                const std::array<float, 2> values =
                    motion_sliding::create(slide_latch, create->right, animation.motion_intent("RightSlide"),
                                           animation.motion_intent("LeftSlide"), sliding);
                animation.emit_packet(encode("slide"), values[0]);
                animation.emit_packet(encode("turn"), values[1]);
                break;
            }
            default: break;
        }
    } else if (std::holds_alternative<Operation::ManualAttribute>(operation.value) && phase == 1) {
        //82BB34D8 uses Manual830BE900; only a negative present value emits.
        if (const std::optional<float> value = animation.motion_intent("Manual"); value && *value < 0.0f) {
            animation.emit_packet(encode("balance"), *value);
        }
    } else if (std::holds_alternative<Operation::Deceleration>(operation.value)) {
        auto* previous = instances.at(behavior).get<Instance::SlideDeceleration>();
        if (previous == nullptr) return rs::make_err(std::string("PowerSlideDecel has an invalid instance"));
        if (phase == 0) {
            //Begin82BB3198; same zero in native instance constructor.
            previous->_0 = 0.0f;
        } else if (phase == 1) {
            if (!fakie_physical) {
                return rs::make_err(std::string("PowerSlideDecel requires actual Motion80 velocity"));
            }
            const std::array<float, 4> velocity = fakie_physical->deck_velocity;
            if (!native_physical) {
                return rs::make_err(std::string("PowerSlideDecel requires actual Reckoning Ground frame"));
            }
            const auto ground = native_physical->board_reckoning;
            const float value = motion_sliding::deceleration(previous->_0, velocity, ground, sliding);
            animation.set_attribute(SettableAttribute{encode("decel"), value, false, -1});
        }
    } else if (std::holds_alternative<Operation::Spin>(operation.value) && phase == 0) {
        S3_MHOST_TRY(const float direction, slide_direction());
        const float value = motion_sliding::spin(direction, sliding);
        animation.set_attribute(SettableAttribute{encode("slidespin"), value, false, -1});
    } else if (std::holds_alternative<Operation::Candidate>(operation.value) && phase != 1) {
        slide_latch.set_candidate_enabled(phase == 0);
    } else if (std::holds_alternative<Operation::IsPowerSliding>(operation.value) && phase != 1) {
        //Begin82BACD20/End82BACD70 -> specific setter8258F910.
        is_power_sliding = phase == 0;
    }
    return rs::Unit{};
}

rs::Result<float, std::string> MotionHost::slide_direction() const {
    if (!fakie_physical) return rs::make_err(std::string("Slide direction requires actual Motion80 velocity"));
    const std::array<float, 4> velocity = fakie_physical->deck_velocity;
    if (!physical || !physical->foot_frame) {
        return rs::make_err(std::string("Slide direction requires actual Motion273 orientation flag"));
    }
    const bool flipped = physical->foot_frame->skateboard_flipped;
    if (!native_physical) return rs::make_err(std::string("Slide direction requires actual Reckoning Ground Z"));
    const std::array<float, 4> z = native_physical->board_reckoning_z;
    return motion_sliding::direction(velocity, z, flipped);
}

}  // namespace s3::game::graph_host::motion
