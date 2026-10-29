#pragma once
#include "../../source/GcLib/portable/ScreenVertex.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

inline void TestScreenVertices() {
    struct Output {
        D3DXVECTOR4 position;
        D3DXVECTOR2 uv;
        D3DXVECTOR4 tint;
        D3DXVECTOR2 pixel;
        float fog = 1;
    };
    struct Input { D3DXVECTOR4 position; std::uint32_t color; D3DXVECTOR2 uv; };
    static_assert(offsetof(Input, color) == 16 && offsetof(Input, uv) == 20);
    std::uint32_t seed = 0x56d31ab9u;
    const auto random = [&]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; };
    unsigned cases = 0;
    for (const auto dimensions : {std::array<unsigned,4>{0,0,640,480}, {11,27,1100,825}, {3,7,1,1}})
    for (bool back : {false,true}) for (bool depth : {false,true}) for (bool textured : {false,true}) {
        const directx::portable::ScreenVertexConverter converter{
            dimensions[0],dimensions[1],dimensions[2],dimensions[3],back,depth};
        for (unsigned i=0;i<1024;++i) {
            Input input;
            input.position = {float(int(random()%40001)-20000)/8,
                              float(int(random()%40001)-20000)/8,
                              float(int(random()%4001)-2000)/1000,
                              i%7==0?0:float(int(random()%2001)-1000)/128};
            input.color = random(); input.uv = {float(random()%8192)/512,float(random()%8192)/512};
            // Padded, deliberately unaligned streams must not become typed
            // pointer loads with stronger alignment than D3D's original API.
            std::array<unsigned char,64> bytes{};
            std::memcpy(bytes.data()+1,&input,sizeof(input));
            Output actual, expected;
            converter.Convert(actual,bytes.data()+1,textured);
            // Preserve the previous generic Decode -> Convert path as an
            // independent reference, including RHW zero and disabled-Z clamp.
            auto position=input.position;
            expected.uv=textured?input.uv:D3DXVECTOR2{};
            expected.tint={float((input.color>>16)&255)/255,float((input.color>>8)&255)/255,
                           float(input.color&255)/255,float((input.color>>24)&255)/255};
            float w=position.w!=0?1/position.w:1;
            const float x=position.x+.5f,y=position.y+.5f;
            const float z=depth?position.z:std::clamp(position.z,0.f,1.f);
            expected.position={(2*(x-dimensions[0])/dimensions[2]-1)*w,
                (back?-1.f:1.f)*(2*(y-dimensions[1])/dimensions[3]-1)*w,(2*z-1)*w,w};
            expected.pixel={position.x,position.y};
            assert(std::memcmp(&actual.position,&expected.position,sizeof(actual.position))==0);
            assert(std::memcmp(&actual.uv,&expected.uv,sizeof(actual.uv))==0);
            assert(std::memcmp(&actual.tint,&expected.tint,sizeof(actual.tint))==0);
            assert(std::memcmp(&actual.pixel,&expected.pixel,sizeof(actual.pixel))==0);
            assert(actual.fog==expected.fog);
            ++cases;
        }
    }
    assert(cases==24576);
}
