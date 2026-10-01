#pragma once
#include "DxMath.hpp"
#include <cstdint>
#include <cstring>

namespace directx::portable {
// A D3D XYZRHW + diffuse (+ optional TEX1) draw has one vertex layout and
// viewport for its entire duration. Validate those once in the caller instead
// of reinterpreting FVF/declarations and constructing skinning data per vertex.
// Keep the original float operation order: reciprocal-based rewrites would
// change subpixel positions and alpha-blended edges.
struct ScreenVertexConverter {
    unsigned viewportX, viewportY, viewportWidth, viewportHeight;
    bool back, depthTest;

    template<class Output>
    void Convert(Output& output, const unsigned char* bytes, bool textured) const {
        D3DXVECTOR4 position;
        std::uint32_t color;
        std::memcpy(&position, bytes, 16);
        std::memcpy(&color, bytes + 16, 4);
        const float w = position.w != 0 ? 1 / position.w : 1;
        const float x = position.x + .5f, y = position.y + .5f;
        const float depth = depthTest ? position.z : std::clamp(position.z, 0.f, 1.f);
        output.position = {
            (2 * (x - viewportX) / viewportWidth - 1) * w,
            (back ? -1.f : 1.f) * (2 * (y - viewportY) / viewportHeight - 1) * w,
            (2 * depth - 1) * w, w};
        output.tint = {float((color >> 16) & 255) / 255,
                       float((color >> 8) & 255) / 255,
                       float(color & 255) / 255, float((color >> 24) & 255) / 255};
        if (textured) std::memcpy(&output.uv, bytes + 20, 8);
        else output.uv = {0, 0};
        output.pixel = {position.x, position.y};
        output.fog = 1;
    }
};
}
