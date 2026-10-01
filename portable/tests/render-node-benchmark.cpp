#include "../../source/GcLib/gstd/SmartPointer.hpp"
#include "../../source/GcLib/gstd/ListNodePool.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <vector>

struct Item { unsigned id; };
using Pointer = gstd::ref_count_ptr<Item>::unsync;
using List = std::list<Pointer>;
static volatile std::uint64_t sink = 0;

template<bool Pooled>
double Measure(std::vector<Pointer>& objects, unsigned frames) {
    std::array<List, 101> buckets;
    gstd::ListNodePool<Pointer, 4096> pool;
    const auto frame = [&]() {
        for (unsigned i = 0; i < objects.size(); ++i) {
            auto& bucket = buckets[(i * 17) % buckets.size()];
            if constexpr (Pooled) pool.Append(bucket, objects[i]);
            else bucket.push_back(objects[i]);
        }
        std::uint64_t checksum = 0;
        for (auto& bucket : buckets) {
            for (auto& object : bucket) checksum += object->id;
            if constexpr (Pooled) pool.Clear(bucket);
            else bucket.clear();
        }
        assert(checksum == objects.size() * (objects.size() - 1) / 2);
        sink = sink + checksum;
    };
    // Warm both the general allocator and the bounded queue before timing.
    for (unsigned i = 0; i < 50; ++i) frame();
    const auto begin = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < frames; ++i) frame();
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    for (auto& object : objects) assert(object.GetReferenceCount() == 1);
    if constexpr (Pooled) assert(pool.CachedNodes() == objects.size());
    return elapsed;
}

int main() {
    constexpr unsigned Frames = 2000;
    std::vector<Pointer> objects;
    for (unsigned i = 0; i < 512; ++i) objects.push_back(new Item{i});
    const bool order[] = {false, true, true, false, true, false, false, true};
    std::vector<double> oldTimes, newTimes;
    std::printf("{\"scope\":\"Wasm queue microbenchmark, not gameplay or phone timing\","
        "\"framesPerSample\":%u,\"objectsPerFrame\":512,\"buckets\":101,\"samples\":[", Frames);
    for (unsigned i = 0; i < 8; ++i) {
        const bool pooled = order[i];
        const double elapsed = pooled ? Measure<true>(objects, Frames) : Measure<false>(objects, Frames);
        (pooled ? newTimes : oldTimes).push_back(elapsed);
        std::printf("%s{\"pooled\":%s,\"ms\":%.6f}", i ? "," : "", pooled ? "true" : "false", elapsed);
    }
    std::sort(oldTimes.begin(), oldTimes.end()); std::sort(newTimes.begin(), newTimes.end());
    std::printf("],\"legacyMedianMs\":%.6f,\"pooledMedianMs\":%.6f,\"retainedNodes\":512,"
        "\"ownershipAndChecksumsMatch\":true}\n",
        (oldTimes[1] + oldTimes[2]) / 2, (newTimes[1] + newTimes[2]) / 2);
}
