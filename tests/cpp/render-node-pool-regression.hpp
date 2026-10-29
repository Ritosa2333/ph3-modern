#pragma once
#include "../../source/GcLib/gstd/ListNodePool.hpp"
#include "../../source/GcLib/gstd/SmartPointer.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

inline void TestRenderNodePool() {
    struct Item {
        int id;
        std::vector<int>* destroyed;
        ~Item() { destroyed->push_back(id); }
    };
    using Pointer = gstd::ref_count_ptr<Item>::unsync;
    using Pool = gstd::ListNodePool<Pointer, 4>;
    std::vector<int> destroyed;
    Pool pool;
    Pool::List a, b;
    auto append = [&](Pool::List& bucket, int id) {
        Pointer item = new Item{id, &destroyed};
        pool.Append(bucket, item);
        assert(item.GetReferenceCount() == 2);
    };
    append(a, 1); append(a, 2); append(a, 3);
    const Pointer* firstNode = &a.front();
    auto stable = a.begin();
    append(a, 4);
    assert((*stable)->id == 1 && &*stable == firstNode);
    pool.Clear(a);
    assert(a.empty() && pool.CachedNodes() == 4);
    assert((destroyed == std::vector<int>{1, 2, 3, 4}));
    append(b, 5);
    assert(&b.front() == firstNode && pool.CachedNodes() == 3);
    append(a, 6); append(b, 7); append(a, 8);
    assert(pool.CachedNodes() == 0);
    assert(a.front()->id == 6 && a.back()->id == 8);
    assert(b.front()->id == 5 && b.back()->id == 7);
    Pointer held = a.front();
    pool.Clear(a);
    assert(held.GetReferenceCount() == 1);
    assert(destroyed.back() == 8);
    held = NULL;
    assert(destroyed.back() == 6);
    pool.Clear(b);
    assert(pool.CachedNodes() == 4 && destroyed.back() == 7);
    const std::size_t before = destroyed.size();
    // A large scene is never truncated; only empty spare storage is bounded.
    for (int i = 0; i < 100; ++i) append(a, 100 + i);
    assert(a.size() == 100 && pool.CachedNodes() == 0);
    pool.Clear(a);
    assert(a.empty() && pool.CachedNodes() == 4);
    assert(destroyed.size() == before + 100);
    for (int i = 0; i < 100; ++i) assert(destroyed[before + i] == 100 + i);
    pool.Clear(a); pool.Clear(b);
    assert(pool.CachedNodes() == 4);
    for (int round = 0; round < 1000; ++round) {
        append(a, round); append(b, round + 1);
        pool.Clear(a); pool.Clear(b);
        assert(pool.CachedNodes() == 4);
    }
    std::puts("PASS render node reuse, order, iterator stability, immediate ownership release and bounded storage");
}
