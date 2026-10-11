// Port of skate-core/src/physics/triangle_sweep_roots.rs.
// Vertex/edge roots in TU3's triangle feature walk:82ADDB48/82ADA3E0.
#pragma once
#include "s3/core/math.hpp"

namespace s3::physics::triangle_sweep_roots {

using math::Vector3;

struct Fraction {
    float numerator = 0.0f;
    float denominator = 0.0f;
    float value() const { return numerator / denominator; }
    bool before(Fraction other) const {
        return numerator * other.denominator < other.numerator * denominator;
    }
};

/// enum Root { Away, Miss, Hit(Fraction) }
struct Root {
    enum class Kind { Away, Miss, Hit } kind = Kind::Miss;
    Fraction fraction;
    static Root Away() { return Root{Kind::Away, {}}; }
    static Root Miss() { return Root{Kind::Miss, {}}; }
    static Root Hit(Fraction f) { return Root{Kind::Hit, f}; }
};

Root sphere(Vector3 start, Vector3 delta, Vector3 center, float radius);
Root cylinder(Vector3 start, Vector3 delta, Vector3 origin, Vector3 edge, float radius);

}  // namespace s3::physics::triangle_sweep_roots
