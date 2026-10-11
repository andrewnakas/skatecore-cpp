// Port of skate-core/src/physics/solver/contact_build.rs.
#include "s3/core/physics/solver/contact_build.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::solver::contact_build {

rs::Result<std::array<float, 3>, ContactBuildError> NativeContactMassResponse::inverse_effective_mass(
    std::array<float, 3> effective_mass) {
    return rs::map(effective_mass, native_arithmetic::reciprocal_estimate);
}

ContactPreparation prepare_contact(const std::array<std::uint32_t, 64>& record, float time_step) {
    return geometry::prepare(input::ContactInput::decode(record), time_step);
}

rs::Result<rs::Unit, ContactBuildError> build_with_response(std::array<std::uint32_t, 64>& record,
                                                            float time_step,
                                                            ContactMassResponse& response) {
    const ContactPreparation prepared = prepare_contact(record, time_step);
    const auto inverse_response = response.inverse_effective_mass(prepared.effective_mass);
    if (inverse_response.is_err()) return rs::make_err(inverse_response.unwrap_err());
    record = publication::compile(prepared, inverse_response.unwrap());
    return rs::Unit{};
}

void build(std::array<std::uint32_t, 64>& record, float time_step) {
    NativeContactMassResponse response;
    if (build_with_response(record, time_step, response).is_err()) {
        S3_PANIC("the native contact mass response is infallible");
    }
}

}  // namespace s3::physics::solver::contact_build
