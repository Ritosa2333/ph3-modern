#pragma once
#include "../../source/GcLib/portable/UniformValueCache.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

inline void TestEffectUniformValues() {
    using Cache = directx::portable::UniformValueCache;
    using Kind = Cache::Kind;
    Cache cache, otherProgram;
    unsigned uploads = 0;
    auto send = [&]() {++uploads;};
    float scalar = 1.0f;
    assert(cache.Apply(0, Kind::Float1, 1, &scalar, sizeof scalar, send));
    assert(!cache.Apply(0, Kind::Float1, 1, &scalar, sizeof scalar, send));
    assert(otherProgram.Apply(0, Kind::Float1, 1, &scalar, sizeof scalar, send));
    assert(uploads == 2);
    assert(!cache.Apply(-1, Kind::Float1, 1, &scalar, sizeof scalar, send));
    assert(!cache.Apply(1, Kind::Float1, 0, nullptr, 0, send));
    assert(cache.Size() == 1);

    // Exact byte identity retains signed zero and different NaN payloads.
    for (std::uint32_t bits : {0u, 0x80000000u, 0x7fc00001u, 0x7fc00002u}) {
        assert(cache.Apply(0, Kind::Float1, 1, &bits, sizeof bits, send));
        assert(!cache.Apply(0, Kind::Float1, 1, &bits, sizeof bits, send));
    }
    std::array<float, 8> vector{1, 2, 3, 4, 5, 6, 7, 8};
    assert(cache.Apply(2, Kind::Float4, 2, vector.data(), sizeof vector, send));
    assert(!cache.Apply(2, Kind::Float4, 2, vector.data(), sizeof vector, send));
    assert(cache.Apply(2, Kind::Float4, 1, vector.data(), sizeof(float) * 4, send));
    assert(cache.Apply(2, Kind::Float4, 2, vector.data(), sizeof vector, send));
    // Same bytes with another uniform operation must not alias the old call.
    assert(cache.Apply(2, Kind::Float2, 4, vector.data(), sizeof vector, send));
    vector[7] = 9;
    assert(cache.Apply(2, Kind::Float2, 4, vector.data(), sizeof vector, send));

    // A sampler/other writer can name an individual element of an array.
    // Uniform locations are opaque; conservatively forget potential aliases.
    assert(cache.Apply(8, Kind::Float1, 1, &scalar, sizeof scalar, send));
    assert(cache.Apply(2, Kind::Float2, 4, vector.data(), sizeof vector, send));
    assert(cache.Apply(8, Kind::Float1, 1, &scalar, sizeof scalar, send));

    std::array<float, 16> matrix{};
    assert(cache.Apply(3, Kind::Matrix4, 1, matrix.data(), sizeof matrix, send));
    for (unsigned i = 0; i < matrix.size(); ++i) {
        matrix[i] = float(i + 1);
        assert(cache.Apply(3, Kind::Matrix4, 1, matrix.data(), sizeof matrix, send));
    }
    assert(!cache.Apply(3, Kind::Matrix4, 1, matrix.data(), sizeof matrix, send));

    // A user parameter may share the reserved alpha-test/sampler location.
    // Both writers must see the last actual value, not separate local caches.
    int user = 1, alpha = 5;
    for (unsigned i = 0; i < 20; ++i) {
        assert(cache.Apply(4, Kind::Int1, 1, &user, sizeof user, send));
        assert(cache.Apply(4, Kind::Int1, 1, &alpha, sizeof alpha, send));
        assert(!cache.Apply(4, Kind::Int1, 1, &alpha, sizeof alpha, send));
    }
    auto failing = []() {throw std::runtime_error("expected upload failure");};
    try {cache.Apply(5, Kind::Float1, 1, &scalar, sizeof scalar, failing); assert(false);}
    catch (const std::runtime_error&) {}
    assert(cache.Apply(5, Kind::Float1, 1, &scalar, sizeof scalar, send));
    cache.Clear(); // Relink/context loss invalidates every old uniform location.
    assert(cache.Size() == 0);
    assert(cache.Apply(0, Kind::Float1, 1, &scalar, sizeof scalar, send));
    const unsigned before = uploads;
    for (unsigned i = 0; i < 10000; ++i)
        assert(!cache.Apply(0, Kind::Float1, 1, &scalar, sizeof scalar, send));
    assert(uploads == before && cache.Size() == 1);
    std::puts("PASS effect uniform identity, shape/type changes, matrix updates, shared overrides, relink and program isolation");
}
