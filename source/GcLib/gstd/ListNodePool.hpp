#pragma once
#include <cstddef>
#include <list>

namespace gstd {
// Reuse storage, not object ownership. Existing list iterators and insertion
// order remain unchanged; clearing a bucket releases every value immediately.
// One bounded spare list is shared by all buckets of the owning manager.
template<class T, std::size_t MaximumNodes>
class ListNodePool {
public:
    using List = std::list<T>;

    void Append(List& destination, const T& value) {
        if (spare_.empty()) {
            destination.push_back(value);
        } else {
            auto node = spare_.begin();
            *node = value;
            destination.splice(destination.end(), spare_, node);
        }
    }

    void Clear(List& source) {
        while (!source.empty() && spare_.size() < MaximumNodes) {
            auto node = source.begin();
            *node = T();
            spare_.splice(spare_.end(), source, node);
        }
        source.clear();
    }

    std::size_t CachedNodes() const { return spare_.size(); }

private:
    List spare_;
};
}
