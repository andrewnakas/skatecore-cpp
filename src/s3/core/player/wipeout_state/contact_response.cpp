// Port of skate-core/src/player/wipeout_state/contact_response.rs (whole file).
#include "s3/core/player/wipeout_state/contact_response.hpp"

namespace s3::player::wipeout_state::contact_response {

using math::dot;
using math::normalize_or;
using math::scale;
using math::sub;

ContactResponseOutput ContactResponse::update(SkeletonBody& body, const ContactResponseInput& input) {
    // 82D3EC58..5C: state selection precedes the first response's update.
    material10_.update(body, input.com_velocity_608, input.material10_normal);
    material11_.update(body, input);
    return ContactResponseOutput{material10_.phase, material10_.finished, material11_.active};
}

V reject(V value, V direction) {
    const V normal = normalize_or(direction, V{});
    return sub(value, scale(normal, dot(value, normal)));
}
V reject_positive(V value, V direction) {
    const float projection = dot(value, normalize_or(direction, V{}));
    if (projection > 0.0f) {
        return sub(value, scale(direction, projection));
    }
    return value;
}
void apply_velocity_delta(SkeletonBody& body, V delta) {
    // All original loops start at part1 and execute23 stores; root0/extras24/25
    // and angular velocities remain untouched. The host stores XYZ explicitly.
    auto& bodies = body.bodies_mut();
    for (std::size_t i = 1; i < 24; ++i) {
        bodies[i].rates.linear_velocity.x += delta[0];
        bodies[i].rates.linear_velocity.y += delta[1];
        bodies[i].rates.linear_velocity.z += delta[2];
    }
}

}  // namespace s3::player::wipeout_state::contact_response
