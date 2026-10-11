// Port of skate-core/src/animation/commands/set_data.rs.
// ACSsetData_job (TU3 0x828D80B8), expressed in bones and typed allocations.
#pragma once
#include <cstdint>
#include <optional>
#include <variant>

#include "s3/core/animation/commands/buffers.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::commands::set_data {

using buffers::BoneBuffers;
using buffers::BoneSlice;
using buffers::BufferError;
using buffers::PoseBuffers;
using output::NativeMatrix;

/// Rust `enum InternalUpdate<T>`.
template <class T>
struct InternalUpdate {
    /// Native opcode 12: byte-sized partial SET count.
    struct Set {
        BoneSlice<T> source;
        std::uint8_t bone_count = 0;
    };
    /// Every other opcode skips the SET copy, but still permits extraction.
    struct Preserve {};

    std::variant<Set, Preserve> value;
    InternalUpdate(Set v) : value(v) {}
    InternalUpdate(Preserve v) : value(v) {}
};

template <class T>
struct CopyCommand {
    InternalUpdate<T> update;
    BoneSlice<T> internal;
    std::optional<BoneSlice<T>> external;
};

/// Rust `enum SetDataCommand { Sqt(CopyCommand<Sqt>), Matrix(CopyCommand<NativeMatrix>) }`.
struct SetDataCommand {
    struct Sqt {
        CopyCommand<output::sqt::Sqt> _0;
    };
    struct Matrix {
        CopyCommand<NativeMatrix> _0;
    };
    std::variant<Sqt, Matrix> value;
    SetDataCommand(Sqt v) : value(v) {}
    SetDataCommand(Matrix v) : value(v) {}
};

/// 828D7E08 uses the low byte of numExtBones when the encoded byte is zero.
inline std::uint8_t resolve_set_count(std::uint8_t encoded_count, std::uint32_t external_bone_count) {
    return encoded_count == 0 ? static_cast<std::uint8_t>(external_bone_count) : encoded_count;
}

namespace detail {
template <class T>
rs::Result<rs::Unit, BufferError> copy(BoneBuffers<T>& buffers, std::uint16_t skeleton_bones,
                                       const CopyCommand<T>& command) {
    if (const auto* set = std::get_if<typename InternalUpdate<T>::Set>(&command.update.value)) {
        auto r = buffers.copy(set->source, command.internal, static_cast<std::size_t>(set->bone_count));
        if (r.is_err()) return rs::make_err(r.unwrap_err());
    }
    if (command.external) {
        // Read internal after SET, including writes through earlier aliases.
        auto r = buffers.copy(command.internal, *command.external, static_cast<std::size_t>(skeleton_bones));
        if (r.is_err()) return rs::make_err(r.unwrap_err());
    }
    return rs::Unit{};
}
}  // namespace detail

/// pub(crate) in Rust.
inline rs::Result<rs::Unit, BufferError> execute(PoseBuffers& buffers, std::uint16_t bone_count,
                                                 Slice<const SetDataCommand> commands) {
    for (const SetDataCommand& command : commands) {
        if (const auto* sqt = std::get_if<SetDataCommand::Sqt>(&command.value)) {
            auto r = detail::copy(buffers.sqt, bone_count, sqt->_0);
            if (r.is_err()) return rs::make_err(r.unwrap_err());
        } else {
            const auto& matrix = std::get<SetDataCommand::Matrix>(command.value);
            auto r = detail::copy(buffers.matrices, bone_count, matrix._0);
            if (r.is_err()) return rs::make_err(r.unwrap_err());
        }
    }
    return rs::Unit{};
}

}  // namespace s3::animation::commands::set_data
