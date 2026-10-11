// Port of skate-core/src/physics/construction/deck_geometry.rs.
#include "s3/core/physics/construction/deck_geometry.hpp"

#include <utility>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::mass::deck_geometry {

using primitive_mass_mod::MassShape;

namespace {

RetailAffineTransform translated(Vector3 translation) {
    RetailAffineTransform t = RetailAffineTransform::IDENTITY;
    t.translation = translation;
    return t;
}

DeckChild fan_triangle(std::pair<float, float> previous, std::pair<float, float> next,
                       std::pair<float, float> slope, float width, float length, float center_z,
                       float fatness, bool front, bool enabled) {
    auto point = [&](std::pair<float, float> arc) {
        const float along_end = arc.first * length;
        const float elevation = front ? along_end : -along_end;
        return Vector3{
            arc.second * width,
            slope.first * elevation,
            rs::mul_add(along_end, slope.second, center_z),
        };
    };
    return DeckChild{
        DeckShape::Triangle{
            {Vector3{0.0f, 0.0f, center_z}, point(previous), point(next)},
            fatness,
            {1.0f, -1.0f, 1.0f},
            0x3E2u,
        },
        RetailAffineTransform::IDENTITY,
        enabled,
    };
}

/// Length path in 82E0B1E0; the shared estimate retains its explicitly marked
/// Xenon arithmetic approximation boundary until hardware validation.
float vector_length_z(float z) {
    const float squared = native_arithmetic::dot3({0.0f, 0.0f, z, 0.0f}, {0.0f, 0.0f, z, 0.0f});
    float inverse = native_arithmetic::reciprocal_square_root_estimate(squared);
    for (int i = 0; i < 2; ++i) {
        const float residual = rs::mul_add(-squared, inverse * inverse, 1.0f);
        inverse = rs::mul_add(inverse * 0.5f, residual, inverse);
    }
    if (squared == 0.0f) {
        return 0.0f;
    }
    return squared * inverse;
}

/// Standalone cosine 82473930. Its even-power tree differs from SinCos.
float cosine(float angle) {
    const float turns = rs::round_ties_even(angle * rs::from_bits(0x3E22F983u));
    const float x = rs::mul_add(-rs::from_bits(0x40C90FDBu), turns, angle);
    const float x2 = x * x;
    const float x4 = x2 * x2;
    const float x6 = x4 * x2;
    const float x8 = x4 * x4;
    const float x10 = x6 * x4;
    const float x12 = x6 * x6;
    const float x14 = x8 * x6;
    const float x16 = x8 * x8;
    const float x18 = x10 * x8;
    const float x20 = x10 * x10;
    const float x22 = x12 * x10;
    float result = rs::mul_add(-0.5f, x2, 1.0f);
    const std::pair<float, std::uint32_t> terms[] = {
        {x4, 0x3D2AAAABu},  {x6, 0xBAB60B61u},  {x8, 0x37D00D01u},  {x10, 0xB493F27Eu},
        {x12, 0x310F76C8u}, {x14, 0xAD49CBA5u}, {x16, 0x29573F9Fu}, {x18, 0xA53413C3u},
        {x20, 0x20F2A15Du}, {x22, 0x9C8671CBu},
    };
    for (const auto& [power, coefficient] : terms) {
        result = rs::mul_add(rs::from_bits(coefficient), power, result);
    }
    return result;
}

}  // namespace

DeckGeometry DeckGeometry::new_(DeckGeometrySettings settings) {
    const float half_width = settings.width * 0.5f;
    const float half_length = settings.mid_length * 0.5f;
    const float half_thickness = settings.thickness * 0.5f;
    const float radius = half_thickness * rs::from_bits(0x3F666666u);
    std::vector<DeckChild> children{DeckChild{
        DeckShape::RoundedBox{
            Vector3{half_width - radius, half_thickness - radius, half_length - radius},
            radius,
        },
        RetailAffineTransform::IDENTITY,
        settings.enable_deck_volume_collisions,
    }};

    // 82E0B1E0: midpoint = first + (second-first)/2. These endpoints are
    // parallel to local Z, so the orientation branch leaves identity.
    const float longitudinal_half_delta = (-half_length - half_length) * 0.5f;
    const float capsule_half_length = vector_length_z(longitudinal_half_delta);
    const float capsule_center_z = half_length + longitudinal_half_delta;
    const float arc_width = half_width - half_thickness;
    for (float x : {arc_width, half_thickness - half_width}) {
        children.push_back(DeckChild{
            DeckShape::Capsule{half_thickness, capsule_half_length},
            translated(Vector3{x, 0.0f, capsule_center_z}),
            settings.enable_end_volume_collisions,
        });
    }
    // 82C0A3E0 retains the sphere constructor's enabled flag, independent
    // of the two authored deck collision toggles.
    for (float z : {rs::from_bits(0x3E70A3D7u), rs::from_bits(0xBE70A3D7u)}) {
        children.push_back(DeckChild{
            DeckShape::Sphere{0.035f},
            translated(Vector3{0.0f, rs::from_bits(0xBCA3D70Au), z}),
            true,
        });
    }

    const float radians = rs::from_bits(0x3C8EFA35u);
    const float back_angle = settings.back_end_angle_degrees * radians;
    const float front_angle = settings.front_end_angle_degrees * radians;
    const std::pair<float, float> back_slope{trigonometry::sin(back_angle), cosine(back_angle)};
    const std::pair<float, float> front_slope{trigonometry::sin(front_angle), cosine(front_angle)};
    const float end_length = settings.back_end_size - half_thickness;
    std::pair<float, float> previous_back{trigonometry::sin(0.0f), cosine(0.0f)};
    std::pair<float, float> previous_front = previous_back;
    for (std::int32_t segment = 1; segment <= settings.end_capsule_count; ++segment) {
        const float back_arc = (static_cast<float>(segment) * rs::from_bits(0xC0490FDBu)) /
                               static_cast<float>(settings.end_capsule_count);
        const std::pair<float, float> next_back{trigonometry::sin(back_arc), cosine(back_arc)};
        children.push_back(fan_triangle(previous_back, next_back, back_slope, arc_width,
                                        end_length, -half_length, half_thickness, false,
                                        settings.enable_deck_volume_collisions));
        previous_back = next_back;
        const float front_arc = (static_cast<float>(segment) * rs::from_bits(0x40490FDBu)) /
                                static_cast<float>(settings.end_capsule_count);
        const std::pair<float, float> next_front{trigonometry::sin(front_arc), cosine(front_arc)};
        children.push_back(fan_triangle(previous_front, next_front, front_slope, arc_width,
                                        end_length, half_length, half_thickness, true,
                                        settings.enable_deck_volume_collisions));
        previous_front = next_front;
    }
    return DeckGeometry{std::move(children)};
}

MassMoments DeckGeometry::mass_moments() const {
    MassMoments total = MassMoments::ZERO;
    for (const auto& child : children) {
        total.add(child.mass_moments());
    }
    // The aggregate volume's transform is identity (82AD7740).
    total.transform(RetailAffineTransform::IDENTITY.basis, Vector3::ZERO);
    return total;
}

MassMoments DeckChild::mass_moments() const {
    MassShape shape;
    RetailAffineTransform transform;
    if (const auto* s = std::get_if<DeckShape::Sphere>(&this->shape.value)) {
        shape = MassShape::Sphere{s->radius};
        transform = this->transform;
    } else if (const auto* c = std::get_if<DeckShape::Capsule>(&this->shape.value)) {
        shape = MassShape::Capsule{c->radius, c->half_length};
        transform = this->transform;
    } else if (const auto* b = std::get_if<DeckShape::RoundedBox>(&this->shape.value)) {
        shape = MassShape::RoundedBox{b->half_extents, b->radius};
        transform = this->transform;
    } else {
        const auto& t = std::get<DeckShape::Triangle>(this->shape.value);
        const auto& vertices = t.vertices;
        const float fatness = t.fatness;
        // TU3 TriangleVolume::GetBBox 82ADDC40, null transform branch:
        // min(v0,min(v1,v2))-fatness, max(v0,max(v1,v2))+fatness.
        // 82AE7058 deliberately uses this box for triangle mass; no
        // triangle pose transform is applied again after this branch.
        auto axis = [&](float Vector3::*get) {
            const float a = vertices[0].*get;
            const float b = vertices[1].*get;
            const float c = vertices[2].*get;
            const float low =
                native_arithmetic::vector_min(a, native_arithmetic::vector_min(b, c)) - fatness;
            const float high =
                native_arithmetic::vector_max(a, native_arithmetic::vector_max(b, c)) + fatness;
            const float half = (high - low) * 0.5f;
            return std::pair<float, float>{half, low + half};
        };
        const auto x = axis(&Vector3::x);
        const auto y = axis(&Vector3::y);
        const auto z = axis(&Vector3::z);
        shape = MassShape::RoundedBox{Vector3{x.first, y.first, z.first}, 0.0f};
        transform = translated(Vector3{x.second, y.second, z.second});
    }
    const auto primitive = primitive_mass_mod::primitive_mass(shape);
    if (!primitive) {
        S3_PANIC("every deck child follows a supported source mass branch");
    }
    MassMoments moments = MassMoments::from_primitive(*primitive);
    moments.transform(transform.basis, transform.translation);
    return moments;
}

}  // namespace s3::physics::mass::deck_geometry
