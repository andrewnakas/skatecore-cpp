// Port of skate-core/src/animation/playback_tree.rs.
#include "s3/core/animation/playback_tree.hpp"

namespace s3::animation::playback_tree {

namespace {
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

float PlaybackTree::length() const {
    return std::visit(overloaded{
                          [](const Clip& v) { return v.clip.clock.length; },
                          [](const PhaseBlend& v) { return v._0.length; },
                          [](const BlendSpace& v) { return v._0.length(); },
                          [](const SelectionSpace& v) {
                              const PlaybackTree* c = v._0.current();
                              return c ? c->length() : 0.0f;
                          },
                          [](const Transition& v) { return v._0.to->length(); },
                          [](const BindPose& v) { return v.motion->length(); },
                      },
                      value);
}

float PlaybackTree::time() const {
    return std::visit(overloaded{
                          [](const Clip& v) { return v.clip.clock.sample_time(); },
                          [](const PhaseBlend& v) { return v._0.time; },
                          [](const BlendSpace& v) { return v._0.time; },
                          [](const SelectionSpace& v) {
                              const PlaybackTree* c = v._0.current();
                              return c ? c->time() : 0.0f;
                          },
                          [](const Transition& v) { return v._0.to->time(); },
                          [](const BindPose& v) { return v.motion->time(); },
                      },
                      value);
}

void PlaybackTree::set_time(float time) {
    std::visit(overloaded{
                   [&](Clip& v) { v.clip.clock.set_time(time); },
                   [&](PhaseBlend& v) { v._0.set_time(time); },
                   [&](BlendSpace& v) { v._0.set_time(time); },
                   [&](SelectionSpace& v) { v._0.set_time(time); },
                   [&](Transition& v) { v._0.to->set_time(time); },
                   [&](BindPose& v) { v.motion->set_time(time); },
               },
               value);
}

void PlaybackTree::set_speed(float speed) {
    std::visit(overloaded{
                   [&](Clip& v) { v.clip.clock.set_speed(speed); },
                   [&](PhaseBlend& v) { v._0.set_speed(speed); },
                   [&](BlendSpace& v) { v._0.set_speed(speed); },
                   [&](SelectionSpace& v) { v._0.set_speed(speed); },
                   [&](Transition& v) { v._0.to->set_speed(speed); },
                   [&](BindPose& v) { v.motion->set_speed(speed); },
               },
               value);
}

void PlaybackTree::advance(float dt, float phase, AdvanceResult& property) {
    std::visit(overloaded{
                   [&](Clip& v) { v.clip.clock.advance(dt, phase, property); },
                   [&](PhaseBlend& v) { v._0.advance(dt, phase, property); },
                   [&](BlendSpace& v) { v._0.advance(dt, phase, property); },
                   [&](SelectionSpace& v) {
                       if (PlaybackTree* child = v._0.current_mut()) {
                           child->advance(dt, phase, property);
                       }
                   },
                   [&](Transition& v) { v._0.advance(dt, phase, property); },
                   [&](BindPose& v) { v.motion->advance(dt, phase, property); },
               },
               value);
}

rs::Result<bool, std::string> PlaybackTree::set_attributes(Slice<const SettableAttribute> attributes) {
    using R = rs::Result<bool, std::string>;
    return std::visit(overloaded{
                          [&](Clip&) -> R { return false; },
                          [&](PhaseBlend& v) -> R { return v._0.set_attributes(attributes); },
                          [&](BlendSpace& v) -> R { return v._0.set_attributes(attributes); },
                          [&](SelectionSpace& v) -> R { return v._0.set_attributes(attributes); },
                          [&](Transition& v) -> R { return v._0.set_attributes(attributes); },
                          [&](BindPose& v) -> R { return v.motion->set_attributes(attributes); },
                      },
                      value);
}

rs::Result<std::vector<AnimationAttribute>, std::string> PlaybackTree::attributes(std::uint32_t mask) const {
    using R = rs::Result<std::vector<AnimationAttribute>, std::string>;
    return std::visit(
        overloaded{
            [&](const Clip& v) -> R { return v.clip.attributes(mask); },
            [&](const PhaseBlend& v) -> R { return v._0.attributes(mask); },
            [&](const BlendSpace& v) -> R { return v._0.attributes(mask); },
            [&](const SelectionSpace& v) -> R {
                const PlaybackTree* c = v._0.current();
                if (!c) return rs::make_err(std::string("SelectionSpace attributes requested before native selection"));
                return c->attributes(mask);
            },
            [&](const Transition& v) -> R { return v._0.attributes(mask); },
            [&](const BindPose& v) -> R {
                auto r = v.motion->attributes(mask);
                if (r.is_err()) return r;
                std::vector<AnimationAttribute> attributes = std::move(r.unwrap());
                for (std::size_t m = 0; m < v.mirror_modes.size(); ++m) {
                    for (auto& attribute : attributes) {
                        auto a = v.attribute_mirror->apply(attribute);
                        if (a.is_err()) return rs::make_err(a.unwrap_err());
                    }
                }
                return attributes;
            },
        },
        value);
}

rs::Result<std::optional<AnimationAttribute>, std::string> PlaybackTree::attribute(AttributeName name,
                                                                                    std::uint32_t mask) const {
    AnimationAttribute output = output::attributes::MotionGraphAttribute{name, 0.0f}.to_animation();
    auto r = query_attribute(name, mask, output);
    if (r.is_err()) return rs::make_err(r.unwrap_err());
    return r.unwrap() ? std::optional<AnimationAttribute>(output) : std::optional<AnimationAttribute>();
}

rs::Result<bool, std::string> PlaybackTree::query_attribute(AttributeName name, std::uint32_t mask,
                                                            AnimationAttribute& output) const {
    using R = rs::Result<bool, std::string>;
    return std::visit(
        overloaded{
            [&](const PhaseBlend& v) -> R { return v._0.query_attribute(name, mask, output); },
            [&](const BlendSpace& v) -> R { return v._0.query_attribute(name, mask, output); },
            [&](const SelectionSpace& v) -> R {
                const PlaybackTree* c = v._0.current();
                if (!c) return rs::make_err(std::string("SelectionSpace attribute requested before native selection"));
                return c->query_attribute(name, mask, output);
            },
            [&](const Transition& v) -> R { return v._0.query_attribute(name, mask, output); },
            [&](const BindPose& v) -> R {
                //825476D0 leaves a child's partial output untouched on false.
                auto r = v.motion->query_attribute(name, mask, output);
                if (r.is_err()) return r;
                const bool found = r.unwrap();
                if (found) {
                    for (std::size_t m = 0; m < v.mirror_modes.size(); ++m) {
                        auto a = v.attribute_mirror->apply(output);
                        if (a.is_err()) return rs::make_err(a.unwrap_err());
                    }
                }
                return found;
            },
            [&](const Clip& v) -> R {
                auto r = v.clip.attribute(name, mask);
                if (r.is_err()) return rs::make_err(r.unwrap_err());
                if (const auto& attribute = r.unwrap()) {
                    output.copy_from(*attribute);
                    return true;
                }
                return false;
            },
        },
        value);
}

rs::Result<bool, std::string> PlaybackTree::evaluate(Evaluation parameters, bool enabled,
                                                     std::vector<PoseCommand>& output) {
    using R = rs::Result<bool, std::string>;
    return std::visit(
        overloaded{
            [&](PhaseBlend& v) -> R { return v._0.evaluate(parameters, enabled, output); },
            [&](BlendSpace& v) -> R { return v._0.evaluate(parameters, enabled, output); },
            [&](SelectionSpace& v) -> R {
                if (!enabled) {
                    return false;
                }
                PlaybackTree* c = v._0.current_mut();
                if (!c) return rs::make_err(std::string("SelectionSpace evaluated before native selection"));
                return c->evaluate(parameters, true, output);
            },
            [&](Transition& v) -> R { return v._0.evaluate(parameters, enabled, output); },
            [&](BindPose& v) -> R {
                auto r = v.motion->evaluate(parameters, enabled, output);
                if (r.is_err()) return r;
                const bool produced = r.unwrap();
                if (produced) {
                    if (v.posture) {
                        output.push_back(PoseCommand::Pose{posture::posture_pose_name(*v.posture)});
                        output.push_back(PoseCommand::Add{true});
                    }
                    output.push_back(PoseCommand::Pose{"RIG_TPOSE"});
                    output.push_back(PoseCommand::Add{true});
                    if (v.board_backwards) {
                        output.push_back(PoseCommand::Pose{"BOARD_BACKWARDS"});
                        output.push_back(PoseCommand::Add{false});
                        output.push_back(PoseCommand::Pose{"BOARD_BACKWARDS_IK"});
                        output.push_back(PoseCommand::Add{true});
                    }
                    for (std::uint32_t trajectory_mode : v.mirror_modes) {
                        output.push_back(PoseCommand::Mirror{trajectory_mode});
                    }
                }
                return produced;
            },
            [&](Clip& v) -> R {
                clip_clock::ClipClock& clock = v.clip.clock;
                const float rate = clock.base_speed * clock.speed;
                const float limit = (clock.frames - 1.0f) / clock.fps;
                auto bound = [limit](float time) {
                    const float lower = -time >= 0.0f ? 0.0f : time;
                    return limit - lower >= 0.0f ? lower : limit;
                };
                float previous_time = bound(clock.previous_time * rate);
                const float time = bound(clock.sample_time() * rate);
                if (clock.loops_since_evaluation == 0 && previous_time > time) {
                    previous_time = time;
                }
                if (enabled) {
                    output.push_back(PoseCommand::Clip{v.name, previous_time, time, clock.loops_since_evaluation});
                }
                if (parameters.update_history) {
                    clock.commit_evaluation();
                }
                return enabled;
            },
        },
        value);
}

}  // namespace s3::animation::playback_tree
