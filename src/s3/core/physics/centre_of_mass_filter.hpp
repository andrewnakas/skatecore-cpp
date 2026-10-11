// Port of skate-core/src/physics/centre_of_mass_filter.rs.
// Physical-output COM conditioning: original TU3 82DE56B0/82DE5978.
// Advances once after physical Skeleton publishes position64 and velocity16.
#pragma once
#include <array>

#include "s3/core/rs.hpp"

namespace s3::physics::centre_of_mass_filter {

struct CentreOfMassOutput {
    /// PhysOutSystemReckoning0, used by animation and camera consumers.
    std::array<float, 4> velocity{};
    /// PhysOutSystemReckoning32.
    std::array<float, 4> acceleration{};
    /// PhysOutSystemReckoning80.
    std::array<float, 4> position{};
};
S3_DERIVE_EQ(CentreOfMassOutput, velocity, acceleration, position)

class CentreOfMassFilter {
public:
    // Default: original Reset82DE5588 clears vectors16/96/112/128 and byte191.
    static CentreOfMassFilter default_() { return CentreOfMassFilter{}; }
    void reset() { *this = default_(); }
    CentreOfMassOutput update(std::array<float, 4> position, std::array<float, 4> velocity);

private:
    std::array<float, 4> velocity_{};
    std::array<float, 4> position_{};
    std::array<float, 4> position_velocity_{};
    std::array<float, 4> accumulated_error_{};
    bool position_valid_ = false;
};

}  // namespace s3::physics::centre_of_mass_filter
