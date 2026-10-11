// Port of skate-game/src/graph_host/motion_animation.rs (out-of-line items).
#include "s3/game/graph_host/motion_animation.hpp"

#include "s3/core/animation/playback_transition.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/player/offboard/reset_animation.hpp"
#include "s3/game/graph_host/motion_animation/tree_builder.hpp"

namespace s3::game::graph_host::motion_animation {

using s3::animation::Box;
using s3::animation::playback_transition::PlaybackTransition;
using s3::animation::skeleton_input::name::encode;

namespace {

AdvanceResult reset_property() { return AdvanceResult{false, -1.0f, -1.0f}; }

bool has_transition(const PlaybackTree& tree) {
    if (std::holds_alternative<PlaybackTree::Transition>(tree.value)) return true;
    if (const auto* t = std::get_if<PlaybackTree::PhaseBlend>(&tree.value)) {
        for (const PlaybackTree& child : t->_0.children)
            if (has_transition(child)) return true;
        return false;
    }
    if (const auto* t = std::get_if<PlaybackTree::BlendSpace>(&tree.value)) {
        for (const PlaybackTree& child : t->_0.children)
            if (has_transition(child)) return true;
        return false;
    }
    if (const auto* t = std::get_if<PlaybackTree::SelectionSpace>(&tree.value)) {
        const PlaybackTree* current = t->_0.current();
        return current != nullptr && has_transition(*current);
    }
    if (const auto* t = std::get_if<PlaybackTree::BindPose>(&tree.value)) return has_transition(*t->motion);
    return false;
}

PlaybackTree prune(PlaybackTree tree) {
    if (auto* t = std::get_if<PlaybackTree::Transition>(&tree.value)) {
        PlaybackTransition& transition = t->_0;
        if (transition.complete()) {
            return prune(std::move(*transition.to));
        }
        *transition.from = prune(std::move(*transition.from));
        *transition.to = prune(std::move(*transition.to));
        return tree;
    }
    if (auto* t = std::get_if<PlaybackTree::BlendSpace>(&tree.value)) {
        for (PlaybackTree& child : t->_0.children) child = prune(std::move(child));
        return tree;
    }
    if (auto* t = std::get_if<PlaybackTree::PhaseBlend>(&tree.value)) {
        for (PlaybackTree& child : t->_0.children) child = prune(std::move(child));
        return tree;
    }
    if (auto* t = std::get_if<PlaybackTree::SelectionSpace>(&tree.value)) {
        for (auto& candidate : t->_0.candidates) *candidate.tree = prune(std::move(*candidate.tree));
        return tree;
    }
    if (auto* t = std::get_if<PlaybackTree::BindPose>(&tree.value)) {
        *t->motion = prune(std::move(*t->motion));
        return tree;
    }
    return tree;
}

}  // namespace

MotionAnimation MotionAnimation::from_metadata(AnimationMetadata metadata) {
    MotionAnimation out;
    out.metadata_ = std::move(metadata);
    out.current = std::nullopt;
    out.current_name = std::nullopt;
    out.motion_intents = IntentMap::new_();
    out.filtered_intents = IntentMap::new_();
    out.grab_type = std::nullopt;
    out.posture = PendingPosture{};
    out.posture_bank_valid = false;
    out.skater_animation_flags = std::nullopt;
    out.natural_stance = 1;
    out.relative_stance = 0;
    out.requested_stance = 0;
    out.reset_action_intents = false;
    out.attribute_mirror = std::make_shared<const AttributeMirror>(AttributeMirror{});
    out.property_ = reset_property();
    return out;
}

void MotionAnimation::seek_current_fraction(float fraction) {
    if (current) current->set_time(current->length() * fraction);
}

std::optional<float> MotionAnimation::pending_parameter(AttributeName name) const {
    for (const SettableAttribute& a : settable.entries()) {
        if (a.name == name) return a.value;
    }
    return std::nullopt;
}

void MotionAnimation::accept_action_graph(const outputs::ActionGraphOutput& output) {
    output.motion_effects.apply_to(motion_intents);
}

void MotionAnimation::reset_from_stock() {
    current = std::nullopt;
    current_name = std::nullopt;
    channels.reset_from_stock();
    grab_type = std::nullopt;
    motion_intents.clear();
    filtered_intents.clear();
    motion_attributes.clear();
    tree_attributes_.clear();
    settable.clear();
    //82B98050 preserves local-player and board-present flags, and saved15200.
    if (skater_animation_flags) *skater_animation_flags &= 0x0ff7ffffu;
    relative_stance = 0;
    reset_action_intents = true;
    property_ = reset_property();
}

rs::Result<rs::Unit, std::string> MotionAnimation::reset_to_given_stance() {
    if (!skater_animation_flags) return rs::make_err(std::string("Stance reset requires SkaterAnim flags"));
    bool relative = relative_stance != 0;
    s3::player::offboard::reset_animation::given_stance(natural_stance, requested_stance, *skater_animation_flags,
                                                        relative);
    relative_stance = relative ? 1u : 0u;
    return rs::Unit{};
}

rs::Result<rs::Unit, std::string> MotionAnimation::set_hierarchy(Slice<const std::string> names,
                                                                Slice<const std::int32_t> mirror_indices) {
    if (names.size() != mirror_indices.size()) {
        return rs::make_err(std::string("Animation hierarchy mirror count differs from bone count"));
    }
    std::vector<std::pair<AttributeName, AttributeName>> pairs;
    pairs.reserve(names.size());
    for (std::size_t index = 0; index < names.size(); ++index) {
        const std::string& name = names[index];
        const std::int32_t i = mirror_indices[index];
        const std::string* target = nullptr;
        if (i == -1) {
            target = &name;
        } else if (i >= 0) {
            target = names.get(static_cast<std::size_t>(i));
            if (!target) return rs::make_err(std::string("Invalid mirrored bone index"));
        } else {
            return rs::make_err(std::string("Invalid negative mirrored bone index"));
        }
        pairs.emplace_back(encode(name), encode(*target));
    }
    attribute_mirror = std::make_shared<const AttributeMirror>(AttributeMirror{std::move(pairs)});
    return rs::Unit{};
}

rs::Result<float, std::string> MotionAnimation::current_time() const {
    if (!current) return rs::make_err(std::string("No current animation tree"));
    return current->time();
}

rs::Result<float, std::string> MotionAnimation::current_length() const {
    if (!current) return rs::make_err(std::string("No current animation tree"));
    return current->length();
}

void MotionAnimation::synchronize_air_time(float fraction) {
    if (current) current->set_time(current->length() * fraction);
}

bool MotionAnimation::in_transition() const { return current.has_value() && has_transition(*current); }

rs::Result<rs::Unit, std::string> MotionAnimation::jump_into(AttributeName name) {
    auto applied = apply_parameters();
    if (applied.is_err()) return applied;
    if (!current) return rs::make_err(std::string("JumpInto requires a current animation tree"));
    AnimationAttribute attribute = MotionGraphAttribute{name, 0.0f}.to_animation();
    auto found = current->query_attribute(name, 31, attribute);
    if (found.is_err()) return rs::make_err(found.unwrap_err());
    if (found.unwrap()) current->set_time(attribute.begin_time);
    return rs::Unit{};
}

void MotionAnimation::advance(float dt, float phase) {
    channels.retire();
    if (current) current = prune(std::move(*current));
    property_ = reset_property();
    if (current) current->advance(dt, phase, property_);
    channels.advance(dt, phase);
}

rs::Result<rs::Unit, std::string> MotionAnimation::apply_parameters() {
    const std::vector<SettableAttribute> attributes = settable.entries().to_vec();
    if (current) {
        auto r = prepare_selection_spaces(*current, attributes);
        if (r.is_err()) return r;
    }
    {
        auto r = channels.prepare_trees([&](PlaybackTree& tree) { return prepare_selection_spaces(tree, attributes); });
        if (r.is_err()) return r;
    }
    if (current) {
        auto r = current->set_attributes(settable.entries());
        if (r.is_err()) return rs::make_err(r.unwrap_err());
    }
    {
        auto r = channels.set_attributes(settable.entries());
        if (r.is_err()) return r;
    }
    settable.clear();
    return rs::Unit{};
}

rs::Result<std::vector<PoseCommand>, std::string> MotionAnimation::evaluate_pose(Evaluation parameters) {
    std::vector<PoseCommand> output;
    if (!current) return rs::make_err(std::string("MotionGraph has not selected an animation"));
    auto r = current->evaluate(parameters, true, output);
    if (r.is_err()) return rs::make_err(r.unwrap_err());
    auto c = channels.evaluate(parameters, output);
    if (c.is_err()) return rs::make_err(c.unwrap_err());
    return output;
}

rs::Result<rs::Unit, std::string> MotionAnimation::refresh_tree_attributes() {
    std::vector<AnimationAttribute> base;
    if (current) {
        auto a = current->attributes(15);
        if (a.is_err()) return rs::make_err(a.unwrap_err());
        base = std::move(a.unwrap());
    }
    auto merged = channels.attributes(std::move(base), 15);
    if (merged.is_err()) return rs::make_err(merged.unwrap_err());
    tree_attributes_ = std::move(merged.unwrap());
    return rs::Unit{};
}

void MotionAnimation::attach(std::string_view intent, AttributeName attribute, bool set) {
    if (const std::optional<float> value = motion_intent(intent)) {
        motion_attributes.push_back(MotionGraphAttribute{attribute, *value});
        if (set) {
            set_attribute(SettableAttribute{attribute, *value, false, -1});
        }
    }
}

void MotionAnimation::set_grab_type(motion_stock_gameplay::GrabType grab) {
    grab_type = grab;
    emit_packet(encode("GrabType"), motion_stock_gameplay::grab_type_as_value(grab));
}

rs::Result<PlaybackTree, std::string> MotionAnimation::build_tree(std::string_view name) const {
    std::vector<std::string> parents;
    return tree_builder::build(metadata_, name, construction_values, parents);
}

rs::Result<bool, std::string> MotionAnimation::new_channel(std::string_view name, std::string_view animation,
                                                           ChannelSettings settings) {
    if (channels.has(name)) return false;
    auto motion = build_tree(animation);
    if (motion.is_err()) return rs::make_err(motion.unwrap_err());
    motion.unwrap().set_speed(settings.speed);
    auto tree = add_bind_pose(std::move(motion.unwrap()), std::nullopt);
    if (tree.is_err()) return rs::make_err(tree.unwrap_err());
    if (settings.mirrored) {
        if (auto* bind = std::get_if<PlaybackTree::BindPose>(&tree.unwrap().value)) bind->mirror_modes.push_back(2);
    }
    channels.insert(std::string(name), std::move(tree.unwrap()), settings);
    return true;
}

rs::Result<bool, std::string> MotionAnimation::transition_channel(std::string_view name, std::string_view animation,
                                                                  ChannelSettings settings,
                                                                  TransitionSettings transition, bool resurrect,
                                                                  bool create_missing) {
    if (!channels.has(name)) {
        if (create_missing) return new_channel(name, animation, settings);
        return false;
    }
    if (!channels.can_transition(name, resurrect)) return false;
    auto motion = build_tree(animation);
    if (motion.is_err()) return rs::make_err(motion.unwrap_err());
    motion.unwrap().set_speed(settings.speed);
    auto tree = add_bind_pose(std::move(motion.unwrap()), std::nullopt);
    if (tree.is_err()) return rs::make_err(tree.unwrap_err());
    if (settings.mirrored) {
        if (auto* bind = std::get_if<PlaybackTree::BindPose>(&tree.unwrap().value)) bind->mirror_modes.push_back(2);
    }
    channels.transition(name, std::move(tree.unwrap()), settings, transition, resurrect);
    return true;
}

rs::Result<PlaybackTree, std::string> MotionAnimation::add_bind_pose(PlaybackTree motion,
                                                                     std::optional<PosturePose> pose) {
    if (!skater_animation_flags) {
        return rs::make_err(std::string("Animation construction requires actual SkaterAnim flags"));
    }
    const std::uint32_t flags = *skater_animation_flags;
    std::vector<std::uint32_t> mirror_modes;
    if ((flags & 0x40000000u) != 0) mirror_modes.push_back(2);
    if ((flags & 0x00400000u) != 0) mirror_modes.push_back(((flags & 0x00200000u) == 0 ? 1u : 0u) + 1u);
    skater_animation_flags = flags & ~0x00600000u;
    if (!mirror_modes.empty() && attribute_mirror->_0.empty()) {
        return rs::make_err(std::string("Mirrored animation requires the stock bone hierarchy"));
    }
    return PlaybackTree(PlaybackTree::BindPose{Box<PlaybackTree>(std::move(motion)), pose,
                                               (flags & 0x80000000u) != 0, std::move(mirror_modes),
                                               attribute_mirror});
}

std::optional<float> MotionAnimation::motion_intent(std::string_view name) const {
    const float* v = motion_intents.get(name);
    return v ? std::optional<float>(*v) : std::nullopt;
}

std::optional<float> MotionAnimation::filtered_intent(std::string_view name) const {
    const float* v = filtered_intents.get(name);
    return v ? std::optional<float>(*v) : std::nullopt;
}

rs::Result<std::optional<AnimationAttribute>, std::string> MotionAnimation::last_attribute(AttributeName name) {
    for (const AnimationAttribute& a : tree_attributes_) {
        if (a.name == name) return std::optional<AnimationAttribute>(a);
    }
    return std::optional<AnimationAttribute>();
}

void MotionAnimation::set_attribute(pushing::SettableAttribute a) {
    set_attribute(SettableAttribute{encode(a.name), a.value, a.normalized, a.sequence_id});
}

void MotionAnimation::set_construction_value(AttributeName name, AttributeName value) {
    for (auto& entry : construction_values) {
        if (entry.first == name) {
            entry.second = value;
            return;
        }
    }
    construction_values.emplace_back(name, value);
}

rs::Result<bool, std::string> MotionAnimation::play(PlaybackRequest request) {
    if (!(request.transition.kind >= 1 && request.transition.kind <= 4)) return false;
    if (request.transition.kind == 1) current = std::nullopt;
    auto built = build_tree(request.animation);
    if (built.is_err()) return rs::make_err(built.unwrap_err());
    //SkaterAnim::GetAnimTree82B980A0 calls AddBindPose82B98118 before
    //Play/Blend set time/speed and before transition initialization.
    using Pair = std::pair<PlaybackTree, std::optional<PosturePose>>;
    Pair pair(std::move(built.unwrap()), std::nullopt);
    if (posture_bank_valid) {
        auto applied = posture.apply<Pair, std::string>(std::move(pair), [](Pair p, PosturePose pose) {
            return rs::Result<Pair, std::string>(Pair(std::move(p.first), pose));
        });
        if (applied.is_err()) return rs::make_err(applied.unwrap_err());
        pair = std::move(applied.unwrap());
    }
    auto bound = add_bind_pose(std::move(pair.first), pair.second);
    if (bound.is_err()) return rs::make_err(bound.unwrap_err());
    PlaybackTree tree = std::move(bound.unwrap());
    tree.set_speed(request.speed);
    std::optional<PlaybackTree> prior = std::move(current);
    current = std::nullopt;
    //82D186F8's no-current branch calls Play with start0.
    if (request.start_time > 0.0f && (request.transition.kind == 1 || prior.has_value())) {
        tree.set_time(request.start_time);
    }
    if (prior) {
        PlaybackTree from = std::move(*prior);
        auto* transition = request.transition.under != 0 ? std::get_if<PlaybackTree::Transition>(&from.value) : nullptr;
        if (transition) {
            PlaybackTree to = std::move(*transition->_0.to);
            *transition->_0.to = PlaybackTree::Transition{
                PlaybackTransition::new_(std::move(to), std::move(tree), request.transition)};
            current = std::move(from);
        } else {
            current = PlaybackTree(
                PlaybackTree::Transition{PlaybackTransition::new_(std::move(from), std::move(tree), request.transition)});
        }
    } else {
        current = std::move(tree);
    }
    current_name = std::move(request.animation);
    return true;
}

}  // namespace s3::game::graph_host::motion_animation
