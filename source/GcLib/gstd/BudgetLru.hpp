#pragma once
#include <cstddef>
#include <list>
#include <map>
#include <utility>

namespace gstd {
// Metadata for opportunistic ownership, separate from explicit pins. The
// caller decides which entries are unused and destroys them at a safe boundary.
template<class Key>
class BudgetLru {
    struct Entry { Key key; std::size_t bytes; };
    using List = std::list<Entry>;
    List recent_;
    std::map<Key, typename List::iterator> index_;
    std::size_t bytes_ = 0;
public:
    BudgetLru() = default;
    BudgetLru(const BudgetLru&) = delete;
    BudgetLru& operator=(const BudgetLru&) = delete;
    std::size_t Size() const { return index_.size(); }
    std::size_t Bytes() const { return bytes_; }
    void Remember(const Key& key, std::size_t bytes) {
        auto found = index_.find(key);
        if (found != index_.end()) {
            bytes_ -= found->second->bytes; found->second->bytes = bytes; bytes_ += bytes;
            recent_.splice(recent_.begin(), recent_, found->second); return;
        }
        recent_.push_front({key, bytes}); index_.emplace(key, recent_.begin()); bytes_ += bytes;
    }
    void Touch(const Key& key) {
        const auto found = index_.find(key);
        if (found != index_.end()) recent_.splice(recent_.begin(), recent_, found->second);
    }
    void Reweight(const Key& key, std::size_t bytes) {
        const auto found = index_.find(key); if (found == index_.end()) return;
        bytes_ -= found->second->bytes; found->second->bytes = bytes; bytes_ += bytes;
    }
    void Forget(const Key& key) {
        const auto found = index_.find(key); if (found == index_.end()) return;
        bytes_ -= found->second->bytes; recent_.erase(found->second); index_.erase(found);
    }
    void Clear() { index_.clear(); recent_.clear(); bytes_ = 0; }
    template<class CanEvict, class Evict>
    std::size_t Trim(std::size_t budget, std::size_t maximumEntries, CanEvict canEvict, Evict evict) {
        std::size_t count = 0; auto cursor = recent_.end();
        while ((bytes_ > budget || index_.size() > maximumEntries) && cursor != recent_.begin()) {
            auto current = --cursor;
            if (!canEvict(current->key)) continue;
            Key key = current->key; bytes_ -= current->bytes; index_.erase(key);
            cursor = recent_.erase(current); ++count; evict(key);
        }
        return count;
    }
};
}
