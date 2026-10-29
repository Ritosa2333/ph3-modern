#include <cassert>
#include <memory>
#include <vector>
#include <cstdio>
#include "../../source/GcLib/gstd/Script.hpp"
#include "../../source/GcLib/gstd/BoundedAllocationCache.hpp"

struct Tracked {
    inline static int alive = 0;
    Tracked() { ++alive; }
    ~Tracked() { --alive; }
};
struct Pooled {
    std::shared_ptr<Tracked> owned;
    static void* operator new(std::size_t) {
        return gstd::BoundedAllocationCache<sizeof(Pooled), 8>::Allocate();
    }
    static void operator delete(void* p) noexcept {
        gstd::BoundedAllocationCache<sizeof(Pooled), 8>::Release(p);
    }
};
struct ResetCount {
    inline static unsigned assignments = 0;
    std::shared_ptr<Tracked> owned;
    ResetCount() = default;
    explicit ResetCount(std::shared_ptr<Tracked> value) : owned(std::move(value)) {}
    ResetCount(const ResetCount&) = default;
    ResetCount& operator=(const ResetCount& other) {
        ++assignments; owned = other.owned; return *this;
    }
};
int main() {
    using Ref = std::shared_ptr<Tracked>;
    {
        gstd::lightweight_vector<Ref> values;
        for (int i = 0; i < 4; ++i) values.push_back(std::make_shared<Tracked>());
        Ref first = values[0];
        values.push_back(values[0]); // aliased argument during capacity growth
        assert(values[4] == first && values.size() == 5 && Tracked::alive == 4);
        values = values; // self-assignment retains the original elements
        assert(values[4] == first && values.size() == 5);
        values.insert(values.begin(), values[3]); // alias while shifting elements
        assert(values[0] == values[4] && values[1] == first);
        first.reset();
        values.clear(); // original VM clear semantics are deliberately retained
        assert(Tracked::alive == 4);
        const auto capacity = values.capacity;
        values.reset_for_reuse(64);
        assert(Tracked::alive == 0 && values.size() == 0 && values.capacity == capacity);
        for (int i = 0; i < 80; ++i) values.push_back(std::make_shared<Tracked>());
        values.reset_for_reuse(64);
        assert(Tracked::alive == 0 && values.capacity == 0 && values.at == nullptr);
        values.push_back(std::make_shared<Tracked>());
        values.erase(values.begin());
        assert(Tracked::alive == 0 && values.size() == 0);
    }
    {
        gstd::lightweight_vector<int> values;
        for (int i = 0; i < 4; ++i) values.push_back(i);
        values.insert(values.begin(), values[3]); // alias with capacity growth
        assert(values[0] == 3 && values[1] == 0 && values[4] == 3);
    }
    using Cache = gstd::BoundedAllocationCache<sizeof(Pooled), 8>;
    {
        gstd::lightweight_vector<ResetCount> values;
        // A popped stack needs no slot resets, even if an earlier deep
        // expression expanded its storage to 64 elements.
        for (int i = 0; i < 33; ++i)
            values.push_back(ResetCount(std::make_shared<Tracked>()));
        while (values.size()) values.pop_back();
        ResetCount::assignments = 0;
        values.reset_for_reuse(64);
        assert(Tracked::alive == 0 && values.capacity == 64);
#ifndef PH3_LEGACY_STACK_RESET
        assert(ResetCount::assignments == 0);
#endif
        // Multiple clear/reuse cycles can hide more than the current length.
        for (int i = 0; i < 9; ++i)
            values.push_back(ResetCount(std::make_shared<Tracked>()));
        values.clear();
        for (int i = 0; i < 2; ++i)
            values.push_back(ResetCount(std::make_shared<Tracked>()));
        values.clear();
        values.push_back(ResetCount(std::make_shared<Tracked>()));
        assert(Tracked::alive == 9);
        ResetCount::assignments = 0;
        values.reset_for_reuse(64);
        assert(Tracked::alive == 0);
#ifndef PH3_LEGACY_STACK_RESET
        assert(ResetCount::assignments == 9);
#endif
        // VM variable assignment may extend length directly inside capacity.
        values.at[6] = ResetCount(std::make_shared<Tracked>());
        values.length = 7;
        values.reset_for_reuse(64);
        assert(Tracked::alive == 0);
        // Copy/assignment copy the active prefix, not hidden unwind values.
        for (int i = 0; i < 8; ++i)
            values.push_back(ResetCount(std::make_shared<Tracked>()));
        values.clear();
        gstd::lightweight_vector<ResetCount> copy(values), assigned;
        assigned = values;
        ResetCount::assignments = 0;
        copy.reset_for_reuse(64); assigned.reset_for_reuse(64);
#ifndef PH3_LEGACY_STACK_RESET
        assert(ResetCount::assignments == 0);
#endif
        assert(Tracked::alive == 8);
        values.reset_for_reuse(64);
        assert(Tracked::alive == 0);
        // Expansion destroys hidden slots instead of carrying stale cleanup
        // bounds into its replacement storage.
        for (int i = 0; i < 3; ++i)
            values.push_back(ResetCount(std::make_shared<Tracked>()));
        values.clear(); values.expand();
        assert(Tracked::alive == 0);
        values.reset_for_reuse(64);
        assert(values.capacity == 0 && values.at == nullptr);
    }
    for (int iteration = 0; iteration < 1000; ++iteration) {
        std::vector<std::unique_ptr<Pooled>> objects;
        for (int i = 0; i < 32; ++i) {
            auto object = std::make_unique<Pooled>(); object->owned = std::make_shared<Tracked>();
            objects.push_back(std::move(object));
        }
        objects.clear();
        assert(Tracked::alive == 0 && Cache::CachedCount() <= 8);
    }
    Cache::Purge(); assert(Cache::CachedCount() == 0);
    std::puts("PASS script containers, dead-environment references, bounded allocation cache");
}
