#pragma once

#include "DxCompat.hpp"
#include <vector>

namespace directx::portable {

// Pixel generation and measurement only. Woo's DxText layout, tags, cache and
// render objects remain the owners of text behavior.
struct GlyphImage {
    int width = 0;
    int height = 0;
    std::vector<DWORD> pixels;
};

SIZE MeasureText(const LOGFONT& font, const wchar_t* text, int count);
bool RegisterFont(const void* bytes, std::size_t size);
GlyphImage RasterizeGlyph(int code, const LOGFONT& font, int borderType,
    int borderWidth, D3DCOLOR top, D3DCOLOR bottom, D3DCOLOR border);

}
