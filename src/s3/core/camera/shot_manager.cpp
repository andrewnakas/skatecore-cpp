// Port of skate-core/src/camera/shot_manager.rs.
#include "s3/core/camera/shot_manager.hpp"

#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/positioner.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::camera {
namespace {

bool eq_ignore_ascii_case(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

}  // namespace

ShotDefinition ShotDefinition::initial() {
    ShotDefinition d;
    d.name = std::string();
    d.shot_type = 0;
    d.shot = Shot::new_();
    d.transition_time = 0.5f;
    d.transition_units = 0;
    d.children = {std::nullopt, std::nullopt, std::nullopt};
    d.blend_points = {0.0f, 0.0f, 0.0f};
    d.blend_value = 0.0f;
    d.blend_type = 0;
    d.blend_smoothing = 0.0f;
    return d;
}

rs::Result<rs::Unit, std::string> ShotNode::setup(const ShotDatabase& database, ShotEnvironment& env,
                                                  std::vector<std::string>& ancestry, std::size_t& count) {
    if (this->definition.shot_type != 1) {
        return rs::Unit{};
    }
    ancestry.push_back(this->definition.name);
    for (const std::optional<std::string>& entry : this->definition.children) {
        if (!entry.has_value()) {
            break;  // take_while(is_some)
        }
        const std::string& name = *entry;
        for (const std::string& parent : ancestry) {
            if (eq_ignore_ascii_case(parent, name)) {
                return rs::make_err(std::string("Cyclic stock camera shot ") + name);
            }
        }
        count += 1;
        if (count > 64) {
            return rs::make_err(std::string("Stock camera shot exceeds its 64-child capacity"));
        }
        S3_CAM_TRY(ShotDefinition loaded, database.load(name));
        ShotNode child = ShotNode::new_(std::move(loaded));
        S3_CAM_TRY(rs::Unit ignored, child.setup(database, env, ancestry, count));
        (void)ignored;
        this->children.push_back(std::move(child));
    }
    ancestry.pop_back();
    this->definition.blend_value =
        env.raw_blend_value(this->definition.blend_type, this->definition.blend_value, 0.0f);
    return rs::Unit{};
}

void ShotNode::update(float dt, ShotEnvironment& env) {
    if (this->definition.shot_type == 0) {
        std::uint32_t entry = this->definition.shot.compass_north;
        entry = (entry == 6 && env.special_camera_flag()) ? 5 : entry;
        const float north = env.compass_north(entry);
        this->definition.shot.update_normal(north, env.heading_mirror());
        return;
    }
    ShotDefinition& definition = this->definition;
    // Empty blended shots never reach the table indexing below in authored
    // stock data. Preserve their native no-publication result.
    if (this->children.empty()) {
        return;
    }
    const float raw = env.raw_blend_value(definition.blend_type, definition.blend_value, dt);
    definition.blend_value = filter_blend_value(definition.blend_value, raw, definition.blend_smoothing,
                                                definition.blend_points[0],
                                                definition.blend_points[this->children.size() - 1]);
    const std::size_t n = this->children.size();
    const auto interval =
        blend_interval(definition.blend_points, {0 < n, 1 < n, 2 < n}, definition.blend_value);
    if (!interval.has_value()) {
        return;
    }
    const std::size_t left = std::get<0>(*interval);
    const std::size_t right = std::get<1>(*interval);
    float fraction = std::get<2>(*interval);
    // 82E06E98 and82E06EC4 are separate multiply/subtract/add operations.
    fraction = trigonometry::sin(fraction * rs::from_bits(0x40490fdb) - rs::from_bits(0x3fc90fdb)) * 0.5f + 0.5f;
    this->children[left].update(dt, env);
    // Native updates the same child twice when both selected pointers match.
    this->children[right].update(dt, env);
    definition.shot.interpolate_from(this->children[left].definition.shot, this->children[right].definition.shot,
                                     fraction);
}

ShotManager ShotManager::new_() {
    ShotManager m;
    m.current_ = ShotNode::new_(ShotDefinition::initial());
    m.previous = Shot::new_();
    m.interpolated = Shot::new_();
    m.elapsed = 0.0f;
    m.transition_distance = 0.0f;
    m.transition_duration = 0.0f;
    m.transition_override = -1.0f;
    m.instant_changes = false;
    return m;
}

void ShotManager::apply_graph_transition(float incoming, float outgoing) {
    if (incoming >= 0.0f) {
        this->current_.definition.transition_time = incoming;
    } else if (this->transition_override >= 0.0f) {
        this->current_.definition.transition_time = this->transition_override;
    }
    this->transition_override = outgoing >= 0.0f ? outgoing : -1.0f;
}

Shot ShotManager::first_leaf() const {
    const ShotNode* node = &this->current_;
    while (!node->children.empty()) {
        node = &node->children.front();
    }
    return node->definition.shot;
}

rs::Result<bool, std::string> ShotManager::set_shot(const std::string& name, bool force,
                                                    const ShotDatabase& database, ShotEnvironment& env,
                                                    ShotPlacement placement) {
    // 82AE8B40 lowercases ASCII A..Z before the native comparison.
    if (!force && eq_ignore_ascii_case(this->current_.definition.name, name)) {
        return false;
    }
    S3_CAM_TRY(ShotDefinition loaded, database.load(name));
    ShotNode current = ShotNode::new_(std::move(loaded));
    this->previous = this->interpolated;
    if (current.definition.shot.use_previous_shot != 0) {
        Shot& shot = current.definition.shot;
        shot.distance = this->interpolated.distance;
        shot.lens_length = this->interpolated.lens_length;
        shot.position_heading = this->interpolated.position_heading;
        shot.position_elevation = this->interpolated.position_elevation;
        shot.framing = this->interpolated.framing;
        shot.arm_orientation = this->interpolated.arm_orientation;
    }
    this->current_ = std::move(current);
    // Native calculates instant time before publishing the new distance.
    if (this->instant_changes) {
        this->make_instant();
    } else {
        this->elapsed = 0.0f;
    }
    std::vector<std::string> ancestry;
    std::size_t count = 0;
    S3_CAM_TRY(rs::Unit ignored, this->current_.setup(database, env, ancestry, count));
    (void)ignored;
    const Shot shot = this->current_.definition.shot;
    const F4 target = position_from_angles(placement.anchor, F4{}, shot.position_elevation, shot.position_heading,
                                           shot.distance, placement.clamp_height, placement.floor_height);
    this->transition_distance = vector_tracker::length(
        rs::from_fn<4>([&](std::size_t i) { return target[i] - placement.camera_position[i]; }));
    return true;
}

float ShotManager::update(float dt, ShotEnvironment& env) {
    this->elapsed = dt + this->elapsed;
    this->current_.update(dt, env);
    this->transition_duration = this->duration(false);
    float fraction = this->transition_duration <= 0.0f ? 1.0f : this->elapsed / this->transition_duration;
    fraction = -fraction >= 0.0f ? 0.0f : fraction;
    fraction = 1.0f - fraction >= 0.0f ? fraction : 1.0f;
    this->interpolated.interpolate_from(this->previous, this->current_.definition.shot, fraction);
    return fraction;
}

void ShotManager::make_instant() {
    this->transition_override = -1.0f;
    this->elapsed = this->duration(false);
}

bool ShotManager::is_transitioning() const { return this->elapsed < this->duration(true); }

float ShotManager::framing_fraction() const {
    const float d = this->duration(true);
    if (d <= 0.0f) {
        return 1.0f;
    }
    const float fraction = manager_state::clamp(this->elapsed / d, 0.0f, 1.0f);
    return rs::mul_add(
        trigonometry::sin(rs::mul_add(fraction, rs::from_bits(0x40490fdb), -rs::from_bits(0x3fc90fdb))), 0.5f, 0.5f);
}

float ShotManager::duration(bool camera_man) const {
    const ShotDefinition& definition = this->current_.definition;
    switch (definition.transition_units) {
        case 1:
            return definition.transition_time * rs::from_bits(0x3c888889);
        case 2: {
            const float speed = (camera_man && definition.transition_time <= 0.0f) ? rs::from_bits(0x38d1b717)
                                                                                    : definition.transition_time;
            return this->transition_distance / speed;
        }
        default:
            return definition.transition_time;
    }
}

}  // namespace s3::camera
