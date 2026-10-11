// Port of skate-core/src/animation/output/intents.rs.
#include "s3/core/animation/output/intents.hpp"

namespace s3::animation::output::intents {

namespace {
std::size_t bucket_index(const IntentName& name) {
    std::uint32_t hash = 0;
    for (std::uint32_t word : name._0) hash += word;  // wrapping_add
    return static_cast<std::size_t>(hash) % BUCKET_COUNT;
}
}  // namespace

std::optional<float> IntentMap::get(IntentName name) const {
    for (const Intent& entry : buckets[bucket_index(name)]) {
        if (entry.name == name) return entry.value;
    }
    return std::nullopt;
}

std::vector<Intent> IntentMap::entries() const {
    std::vector<Intent> out;
    out.reserve(len_);
    for (const auto& bucket : buckets) {
        for (const Intent& entry : bucket) out.push_back(entry);
    }
    return out;
}

rs::Result<bool, IntentCapacityExceeded> IntentMap::insert(Intent intent) {
    auto& bucket = buckets[bucket_index(intent.name)];
    for (const Intent& entry : bucket) {
        if (entry.name == intent.name) return false;
    }
    if (len_ == ENTRY_CAPACITY) {
        return rs::make_err(IntentCapacityExceeded{});
    }
    bucket.insert(bucket.begin(), intent);
    len_ += 1;
    return true;
}

rs::Result<bool, IntentCapacityExceeded> IntentMap::set(IntentName name, float value) {
    auto& bucket = buckets[bucket_index(name)];
    for (Intent& entry : bucket) {
        if (entry.name == name) {
            entry.value = value;
            return false;
        }
    }
    return insert(Intent{name, value});
}

bool IntentMap::remove(IntentName name) {
    auto& bucket = buckets[bucket_index(name)];
    for (auto it = bucket.begin(); it != bucket.end(); ++it) {
        if (it->name == name) {
            bucket.erase(it);
            len_ -= 1;
            return true;
        }
    }
    return false;
}

rs::Result<bool, IntentCapacityExceeded> IntentMap::apply(IntentName name,
                                                          const IntentMutation& mutation) {
    if (std::get_if<IntentMutation::None>(&mutation.value)) return false;
    if (std::get_if<IntentMutation::Remove>(&mutation.value)) return remove(name);
    const auto& set_ = std::get<IntentMutation::Set>(mutation.value);
    return set(name, set_._0);
}

void IntentMap::clear() {
    for (auto& bucket : buckets) bucket.clear();
    len_ = 0;
}

void IntentMap::replace_from(const IntentMap& source) {
    clear();
    for (const Intent& entry : source.entries()) {
        // Both maps enforce the same64-entry bound and exact key uniqueness.
        if (insert(entry).is_err()) S3_PANIC("bounded intent map assignment");
    }
}

}  // namespace s3::animation::output::intents
