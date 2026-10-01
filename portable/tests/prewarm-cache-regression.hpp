#pragma once
#include "../../source/GcLib/gstd/PrewarmCacheScope.hpp"
#include "../../source/GcLib/gstd/SmartPointer.hpp"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>

inline void TestPrewarmCacheScope() {
    struct Engine {
        int* destroyed;
        ~Engine() {++*destroyed;}
    };
    using Pointer = gstd::ref_count_ptr<Engine>::unsync;
    int destroyed = 0, observed = 0;
    std::map<std::string, Pointer> cache;
    gstd::PrewarmCacheScope<std::string> scope;
    cache["ordinary-cache-entry"] = new Engine{&destroyed};
    cache["previous-prewarm"] = new Engine{&destroyed};
    cache["live-previous-script"] = new Engine{&destroyed};
    Pointer live = cache["live-previous-script"];
    scope.Remember("previous-prewarm");
    scope.Remember("previous-prewarm"); // Repeated loads do not duplicate ownership.
    scope.Remember("live-previous-script");
    scope.Remember("already-removed");
    auto released = [&](Pointer& engine) {assert(engine.GetReferenceCount() == 1); ++observed;};
    assert(scope.RetireUnused(cache, released) == 1);
    assert(observed == 1 && destroyed == 1 && scope.Size() == 1);
    assert(cache.size() == 2 && cache.count("ordinary-cache-entry") == 1);
    assert(live.GetReferenceCount() == 2);
    // The new stage keeps all of its prewarm, even before any gameplay client
    // starts. Retiring is explicit and occurs only at the next stage boundary.
    for (int i = 0; i < 100; ++i) {
        const auto key = "new-stage-" + std::to_string(i);
        cache[key] = new Engine{&destroyed};
        scope.Remember(key);
    }
    assert(destroyed == 1 && cache.size() == 102);
    live = NULL;
    assert(scope.RetireUnused(cache, released) == 101);
    assert(observed == 102 && destroyed == 102 && scope.Size() == 0);
    assert(cache.size() == 1 && cache.count("ordinary-cache-entry") == 1);
    assert(scope.RetireUnused(cache, released) == 0);
    scope.Remember("ordinary-cache-entry"); scope.Clear();
    assert(scope.RetireUnused(cache, released) == 0 && cache.size() == 1);
    cache.clear(); assert(destroyed == 103);
    std::puts("PASS stage-scoped prewarm retirement, live client protection, unmarked cache preservation and delayed release");
}
