#pragma once
#include "../../source/GcLib/portable/DrawTransforms.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

inline void TestDrawTransforms() {
    struct State {
        std::array<D3DXMATRIX,3> matrices;
        mutable unsigned lookups = 0;
        D3DXMATRIX Transform(unsigned key) const {
            assert(key >= 11 && key <= 13);
            ++lookups;
            return matrices[key - 11];
        }
    } state;
    std::uint32_t seed = 0x752391abu;
    const auto random = [&]() {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        return float(int(seed % 8193) - 4096) / 512.f;
    };
    unsigned cases = 0;
    for (unsigned draw = 0; draw < 128; ++draw) {
        for (auto& matrix : state.matrices) {
            std::array<float,16> values;
            for (float& v : values) v = random();
            static_assert(sizeof(matrix) == sizeof(values));
            std::memcpy(&matrix,values.data(),sizeof(matrix));
        }
        // A new draw owns a fresh cache even when the previous draw used the
        // same device. Preparing a cache performs no arithmetic on matrices.
        directx::portable::DrawTransforms cache;
        const unsigned before = state.lookups;
        assert(!cache.prepared && state.lookups == before);
        for (unsigned vertex = 0; vertex < 128; ++vertex) {
            cache.Prepare(state,11,12,13);
            assert(std::memcmp(&cache.world,&state.matrices[0],sizeof(D3DXMATRIX)) == 0);
            assert(std::memcmp(&cache.view,&state.matrices[1],sizeof(D3DXMATRIX)) == 0);
            assert(std::memcmp(&cache.projection,&state.matrices[2],sizeof(D3DXMATRIX)) == 0);
            const D3DXVECTOR4 input{random(),random(),random(),random()};
            D3DXVECTOR4 actual = input, expected = input;
            D3DXVec4Transform(&actual,&actual,&cache.world);
            D3DXVec4Transform(&actual,&actual,&cache.view);
            D3DXVec4Transform(&actual,&actual,&cache.projection);
            auto world = state.matrices[0], view = state.matrices[1], projection = state.matrices[2];
            D3DXVec4Transform(&expected,&expected,&world);
            D3DXVec4Transform(&expected,&expected,&view);
            D3DXVec4Transform(&expected,&expected,&projection);
            assert(std::memcmp(&actual,&expected,sizeof(actual)) == 0);
            ++cases;
        }
#ifndef PH3_LEGACY_VERTEX_CONSTANTS
        assert(state.lookups == before + 3);
#else
        assert(state.lookups == before + 3 * 128);
#endif
    }
    assert(cases == 16384);
}
