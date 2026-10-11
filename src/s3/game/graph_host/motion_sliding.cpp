// Port of skate-game/src/graph_host/motion_sliding.rs (module `motion_sliding`).
#include "s3/game/graph_host/motion_sliding.hpp"

#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_sliding {

using s3::riding::ground_correction_math::dot_product;

rs::Result<Settings, std::string> Settings::load(const s3::data::Collections& data) {
    auto f = [&](const char* name) { return motion_collections::float_(data, "anim_motion", "power_slide", name); };
    auto curve = [&](const char* name) -> rs::Result<PointGraph<4>, std::string> {
        S3_MOTION_TRY(const auto w, motion_collections::words<12>(data, "anim_motion", "power_slide", name));
        return motion_collections::graph<4>(w, 4, 8);
    };
    auto curve8 = [&](const char* name) -> rs::Result<PointGraph<8>, std::string> {
        S3_MOTION_TRY(const auto w, motion_collections::words<20>(data, "anim_motion", "power_slide", name));
        return motion_collections::graph<8>(w, 4, 12);
    };
    Settings s;
    S3_MOTION_TRY(s.speed_threshold, f("slide_speed_threshold"));
    S3_MOTION_TRY(s.well_into_slide, f("well_into_slide_time"));
    S3_MOTION_TRY(s.speed_factor, curve("slide_speed_factor"));
    S3_MOTION_TRY(s.time_threshold, curve("not_sliding_time_thresh"));
    S3_MOTION_TRY(s.threshold, curve("not_sliding_threshold"));
    // Native layout0,80,1688,1692,1680 respectively.
    S3_MOTION_TRY(s.speed_to_lean, curve8("slide_speed_to_lean"));
    S3_MOTION_TRY(s.phys_to_anim_spin, curve8("slide_phys_to_anim_spin"));
    S3_MOTION_TRY(s.smooth, f("slide_smooth"));
    S3_MOTION_TRY(s.minimum_entry, f("slide_min_entry"));
    S3_MOTION_TRY(s.turn, f("slide_turn_value"));
    return s;
}

std::optional<Operation> Operation::parse(const Attributes& a) {
    const std::string* name = a.text("name");
    if (name == nullptr) return std::nullopt;
    const std::string& n = *name;
    if (n == "PowerSliding") return Operation(Update{});
    // Factory82BC9100 explicitly defaults authored right to true.
    if (n == "CreateSlide") return Operation(Create{a.boolean_byte("right", 1) != 0});
    if (n == "PowerSlideManualAtt") return Operation(ManualAttribute{});
    if (n == "PowerSlideDecel") return Operation(Deceleration{});
    if (n == "PowerSlideSpin") return Operation(Spin{});
    if (n == "InCandidateSlidingState") return Operation(Candidate{});
    if (n == "IsPowerSliding") return Operation(IsPowerSliding{});
    return std::nullopt;
}

void State::update(const MotionAnimation& animation, std::uint32_t category, float speed, float direction_, float dt,
                   const Settings& settings, SlideLatch& output) {
    const std::optional<float> right = animation.motion_intent("RightSlide");
    const std::optional<float> left = animation.motion_intent("LeftSlide");
    const float right_value = right.value_or(0.0f);
    const float left_value = left.value_or(0.0f);
    const bool ground = category == 1;
    if (ground && !was_ground) {
        not_sliding_time = 0.0f;
        previous_right = right_value;
        previous_left = left_value;
        was_right_start = false;
        was_left_start = false;
    }
    was_ground = ground;
    if (!ground) return;
    const bool fast = speed > settings.speed_threshold;
    const bool right_start = animation.motion_intent("RightSlideStart").has_value();
    output.set_start(true, fast && right_start && !was_right_start && previous_right < right_value);
    previous_right = right_value;
    was_right_start = right_start;
    const bool left_start = animation.motion_intent("LeftSlideStart").has_value();
    output.set_start(false, fast && left_start && !was_left_start && previous_left > left_value);
    previous_left = left_value;
    was_left_start = left_start;
    const float limit = settings.time_threshold.evaluate(direction_);
    const float sliding = settings.speed_factor.evaluate(speed) * (1.0f - rs::abs(direction_));
    not_sliding_time = sliding >= settings.threshold.evaluate(direction_) ? 0.0f : not_sliding_time + dt;
    const bool stop = not_sliding_time > limit;
    output.advance_elapsed(true, dt);
    output.advance_elapsed(false, dt);
    output.set_end(true, output.elapsed(true) > settings.well_into_slide && (!right || !fast || stop));
    output.set_end(false, output.elapsed(false) > settings.well_into_slide && (!left || !fast || stop));
}

float direction(std::array<float, 4> velocity, std::array<float, 4> z, bool flipped) {
    const float squared = dot_product(velocity, velocity);
    //825959D4 ble also takes the unordered branch after fcmpu.
    if (!(squared > rs::from_bits(0x3a83126fu))) return 1.0f;
    if (flipped) z = {-z[0], -z[1], -z[2], -z[3]};
    float root = 1.0f / rs::sqrt(squared);
    for (int i = 0; i < 2; ++i) {
        const float error = rs::mul_add(-squared, root * root, 1.0f);
        root = rs::mul_add(root * 0.5f, error, root);
    }
    const float length = squared * root;
    float inverse = 1.0f / length;
    for (int i = 0; i < 2; ++i) inverse = rs::mul_add(inverse, rs::mul_add(-inverse, length, 1.0f), inverse);
    return dot_product({velocity[0] * inverse, velocity[1] * inverse, velocity[2] * inverse, velocity[3] * inverse}, z);
}

std::array<float, 2> create(const SlideLatch& latch, bool authored_right, std::optional<float> right_intent,
                            std::optional<float> left_intent, const Settings& settings) {
    const bool right = authored_right ^ latch.captured_fakie();
    float slide = (right ? right_intent : left_intent).value_or(0.0f);
    if (!(latch.elapsed(right) > settings.well_into_slide)) {
        if (right) {
            slide = settings.minimum_entry - slide >= 0.0f ? settings.minimum_entry : slide;
        } else {
            const float minimum = -settings.minimum_entry;
            slide = minimum - slide >= 0.0f ? slide : minimum;
        }
    }
    float turn = right ? settings.turn : -settings.turn;
    if (latch.captured_fakie()) {
        turn = -turn;
        const float negative = -slide;
        const float absolute = rs::abs(negative);
        const float wrapped = absolute > 0.5f ? 1.0f - absolute : absolute;
        slide = negative >= 0.0f ? wrapped : -wrapped;
    }
    return {slide, turn};
}

float deceleration(float& previous, std::array<float, 4> velocity, const std::array<std::array<float, 4>, 4>& ground,
                   const Settings& settings) {
    std::array<float, 4> local = rs::from_fn<4>([&](std::size_t lane) {
        const float x = ground[lane][0] * velocity[0];
        const float y = rs::mul_add(ground[lane][1], velocity[1], x);
        return rs::mul_add(ground[lane][2], velocity[2], y);
    });
    local[1] = 0.0f;
    const float squared = dot_product(local, local);
    float length = 0.0f;
    if (squared != 0.0f) {
        float inverse = 1.0f / rs::sqrt(squared);
        for (int i = 0; i < 2; ++i) {
            const float error = rs::mul_add(-squared, inverse * inverse, 1.0f);
            inverse = rs::mul_add(inverse * 0.5f, error, inverse);
        }
        length = squared * inverse;
    }
    const float target = settings.speed_to_lean.evaluate(length);
    const float retained = (1.0f - settings.smooth) * previous;
    previous = rs::mul_add(target, settings.smooth, retained);
    return previous;
}

}  // namespace s3::game::graph_host::motion_sliding
