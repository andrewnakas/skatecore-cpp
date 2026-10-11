// Port of skate-core/src/animation/output/attributes.rs.
// Attribute copying and ordered packet merge from TU3 8258EED0/8258F210.
// Binary names stay at this typed boundary. This module does not interpret
// Skeleton attributes or invent a physics effect from their names.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::output::attributes {

/// Rust tuple struct `AttributeName(pub [u32; 5])`; `_0` is Rust's `.0`.
struct AttributeName {
    std::array<std::uint32_t, 5> _0{};
};
S3_DERIVE_EQ(AttributeName, _0)

/// The six union lanes retain initialization separately from value. Copying a
/// scalar must not fabricate values for the other five lanes. Words preserve
/// all floating-point payload bits across non-arithmetic publication.
/// Rust tuple struct `AttributePayload(pub [Option<u32>; 6])`.
struct AttributePayload {
    std::array<std::optional<std::uint32_t>, 6> _0{};
};
S3_DERIVE_EQ(AttributePayload, _0)

struct AnimationAttribute {
    AttributePayload payload;
    float begin_time = 0.0f;
    float end_time = 0.0f;
    AttributeName name;
    std::uint8_t status = 0;
    /// Native+53:0/2 scalar,1 vector,3 six-word payload; other kinds copy none.
    std::uint8_t kind = 0;
    std::int32_t sequence_id = 0;

    /// Complete observable field assignment8258EED0. Native padding at54/55
    /// and60..63 is not copied and has no represented domain field.
    void copy_from(const AnimationAttribute& source) {
        kind = source.kind;
        name = source.name;
        begin_time = source.begin_time;
        end_time = source.end_time;
        status = source.status;
        sequence_id = source.sequence_id;
        std::size_t lanes = 0;
        switch (kind) {
            case 0:
            case 2: lanes = 1; break;
            case 1: lanes = 4; break;
            case 3: lanes = 6; break;
            default: lanes = 0; break;
        }
        for (std::size_t i = 0; i < lanes; ++i) payload._0[i] = source.payload._0[i];
    }

    /// Copy-construction8258EE68 initializes name storage then performs the
    /// assignment above. Its inactive union lanes are unspecified, not zero.
    /// (Private in Rust.)
    static AnimationAttribute copy_construct(const AnimationAttribute& source) {
        AnimationAttribute result = source;
        result.payload = AttributePayload{};
        result.copy_from(source);
        return result;
    }
};
S3_DERIVE_EQ(AnimationAttribute, payload, begin_time, end_time, name, status, kind, sequence_id)

struct MotionGraphAttribute {
    /// Native24-byte MG record:20-byte name followed by scalar.
    AttributeName name;
    float value = 0.0f;

    ///8258F210 makes an untimed scalar attribute with status6 and sequence-1.
    AnimationAttribute to_animation() const {
        AttributePayload payload{};
        payload._0[0] = rs::to_bits(value);
        AnimationAttribute out;
        out.payload = payload;
        out.begin_time = -1.0f;
        out.end_time = -1.0f;
        out.name = name;
        out.status = 6;
        out.kind = 0;
        out.sequence_id = -1;
        return out;
    }
};
S3_DERIVE_EQ(MotionGraphAttribute, name, value)

/// Active entries plus retained slots model native end=begin list reset.
/// Capacity allocation policy is host-owned; active ordering and union-lane
/// preservation follow native append/copy semantics. No sorting/deduplication.
struct PacketAttributes {
    Slice<const AnimationAttribute> entries() const {
        return Slice<const AnimationAttribute>(slots.data(), active_len);
    }
    void clear() { active_len = 0; }
    void append(const AnimationAttribute& source) {
        if (active_len < slots.size()) {
            slots[active_len].copy_from(source);
        } else {
            slots.push_back(AnimationAttribute::copy_construct(source));
        }
        active_len += 1;
    }

    /// Exact list operation order at82593750..825937E4: discard prior active
    /// list, append converted MG records, then tree records in stored order.
    /// Duplicate names remain distinct records for the later Skeleton dispatch.
    void replace_from(Slice<const MotionGraphAttribute> motion_graph,
                      Slice<const AnimationAttribute> tree) {
        clear();
        for (const MotionGraphAttribute& attribute : motion_graph) {
            append(attribute.to_animation());
        }
        for (const AnimationAttribute& attribute : tree) {
            append(attribute);
        }
    }

    // Private in Rust.
    std::vector<AnimationAttribute> slots;
    std::size_t active_len = 0;
};

}  // namespace s3::animation::output::attributes
