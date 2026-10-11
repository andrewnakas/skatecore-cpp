// Port of skate-core/src/camera/shot_manager.rs.
//! Normal camera ShotManager82E06370/82E064B0/82E068B8/82E06E38.
//! The host owns immutable stock definitions and tree allocation. The authored
//! child order, filter history and transition calculations retain TU3 behavior.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/shot.hpp"

namespace s3::camera {

struct ShotDefinition {
    std::string name;
    std::uint32_t shot_type = 0;
    Shot shot;
    float transition_time = 0.0f;
    std::uint32_t transition_units = 0;
    std::array<std::optional<std::string>, 3> children{};
    std::array<float, 3> blend_points{};
    float blend_value = 0.0f;
    std::uint32_t blend_type = 0;
    float blend_smoothing = 0.0f;

    static ShotDefinition initial();  // private in Rust
};
S3_DERIVE_EQ(ShotDefinition, name, shot_type, shot, transition_time, transition_units, children, blend_points,
             blend_value, blend_type, blend_smoothing)

struct ShotDatabase {
    virtual ~ShotDatabase() = default;
    virtual rs::Result<ShotDefinition, std::string> load(const std::string& name) const = 0;
};

/// Runtime values are produced by CameraMan82E07708 and the subject compass.
/// The 6->5 compass override is applied here only when its subject flag is set.
struct ShotEnvironment {
    virtual ~ShotEnvironment() = default;
    virtual float heading_mirror() const = 0;
    virtual float compass_north(std::uint32_t entry) const = 0;
    virtual bool special_camera_flag() const = 0;
    virtual float raw_blend_value(std::uint32_t kind, float authored, float dt) = 0;
};

struct ShotPlacement {
    /// Native rig transform+48, not the anchor tracker's smoothed position.
    F4 anchor{};
    F4 camera_position{};
    std::uint8_t clamp_height = 0;
    float floor_height = 0.0f;
};
S3_DERIVE_EQ(ShotPlacement, anchor, camera_position, clamp_height, floor_height)

/// private in Rust.
struct ShotNode {
    ShotDefinition definition;
    std::vector<ShotNode> children;

    static ShotNode new_(ShotDefinition definition) { return ShotNode{std::move(definition), {}}; }
    rs::Result<rs::Unit, std::string> setup(const ShotDatabase& database, ShotEnvironment& env,
                                            std::vector<std::string>& ancestry, std::size_t& count);
    void update(float dt, ShotEnvironment& env);
};
inline bool operator==(const ShotNode& a, const ShotNode& b) {
    return a.definition == b.definition && a.children == b.children;
}
inline bool operator!=(const ShotNode& a, const ShotNode& b) { return !(a == b); }

struct ShotManager {
    ShotNode current_;  // private `current` in Rust
    Shot previous;
    Shot interpolated;
    float elapsed = 0.0f;
    float transition_distance = 0.0f;
    float transition_duration = 0.0f;
    float transition_override = 0.0f;
    bool instant_changes = false;

    static ShotManager new_();
    const ShotDefinition& current() const { return this->current_.definition; }

    /// CameraChooseShot::Begin82DF5C60, after SetShot. The previous behavior's
    /// outgoing override is used only when this one has no incoming override.
    void apply_graph_transition(float incoming, float outgoing);
    /// CameraMan reset follows child0 pointers to the authored leaf.
    Shot first_leaf() const;
    rs::Result<bool, std::string> set_shot(const std::string& name, bool force, const ShotDatabase& database,
                                           ShotEnvironment& env, ShotPlacement placement);
    float update(float dt, ShotEnvironment& env);
    void make_instant();
    /// CameraMan82DFF174 clamps a distance-transition speed <=0 to0.0001;
    /// ShotManager82E06418 and MakeInstant82E07450 do not apply that clamp.
    bool is_transitioning() const;
    /// CameraMan82DFF964..82DFFA08 uses a separate sine-eased framing blend.
    float framing_fraction() const;

private:
    float duration(bool camera_man) const;
};
S3_DERIVE_EQ(ShotManager, current_, previous, interpolated, elapsed, transition_distance, transition_duration,
             transition_override, instant_changes)

}  // namespace s3::camera
