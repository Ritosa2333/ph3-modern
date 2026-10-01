#pragma once
#include <cstddef>
#include <set>

namespace gstd {
// Prewarming is a stage-local optimization, not ownership of every script in
// the campaign. Only explicitly remembered entries may be retired, and a live
// client always wins over reclamation. No mid-stage eviction is performed.
template<class Key>
class PrewarmCacheScope {
public:
    void Remember(const Key& key) { keys_.insert(key); }
    void Clear() { keys_.clear(); }
    std::size_t Size() const { return keys_.size(); }

    template<class Map, class OnRelease>
    std::size_t RetireUnused(Map& cache, OnRelease onRelease) {
        std::size_t retired = 0;
        for (auto key = keys_.begin(); key != keys_.end();) {
            auto found = cache.find(*key);
            if (found == cache.end()) {
                key = keys_.erase(key);
            } else if (found->second == nullptr || found->second.GetReferenceCount() == 1) {
                onRelease(found->second);
                cache.erase(found);
                key = keys_.erase(key);
                ++retired;
            } else {
                // Still used by an old-stage task or by the package. Keep its
                // provenance so a later stage boundary can reclaim it safely.
                ++key;
            }
        }
        return retired;
    }

private:
    std::set<Key> keys_;
};
}
