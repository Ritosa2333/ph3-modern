#include "Font.hpp"
#include "../gstd/GstdUtility.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include FT_SFNT_NAMES_H

namespace directx::portable {
namespace {

class FontFace {
public:
    FontFace() {
        if (FT_Init_FreeType(&library_))
            throw std::runtime_error("FreeType initialization failed");
        // The local shell mounts the user's private font, not a bundled font.
        if (FT_New_Face(library_, "/game/msgothic.ttc", 0, &face_)) {
            FT_Done_FreeType(library_);
            library_ = nullptr;
            throw std::runtime_error("Japanese font /game/msgothic.ttc is missing or invalid");
        }
        FT_Select_Charmap(face_, FT_ENCODING_UNICODE);
        AddNames(face_);
    }
    ~FontFace() {
        for (const auto& item : additional_) FT_Done_Face(item.face);
        if (face_) FT_Done_Face(face_);
        if (library_) FT_Done_FreeType(library_);
    }
    FT_Face Select(const LOGFONT& font) {
        FT_Face selected = face_;
        auto found = facesByName_.find(Normalize(font.lfFaceName));
        if (found != facesByName_.end()) selected = found->second;
        const int height = std::abs(font.lfHeight);
        if (height < 1 || height > 1024)
            throw std::runtime_error("Font height must be between 1 and 1024 pixels");
        FT_Size_RequestRec size{};
        // Positive LOGFONT height includes the cell; negative height is em size.
        size.type = font.lfHeight > 0 ? FT_SIZE_REQUEST_TYPE_REAL_DIM : FT_SIZE_REQUEST_TYPE_NOMINAL;
        size.height = height * 64;
        if (FT_Request_Size(selected, &size))
            throw std::runtime_error("FreeType font size selection failed");
        return selected;
    }
    bool Add(const void* data, std::size_t size) {
        if (!data || !size || size > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max())) return false;
        auto bytes = std::make_shared<std::vector<BYTE>>(static_cast<const BYTE*>(data), static_cast<const BYTE*>(data) + size);
        FT_Face first = nullptr;
        if (FT_New_Memory_Face(library_, bytes->data(), static_cast<FT_Long>(size), 0, &first)) return false;
        const FT_Long count = first->num_faces;
        FT_Select_Charmap(first, FT_ENCODING_UNICODE);
        additional_.push_back({first, bytes}); AddNames(first);
        for (FT_Long index = 1; index < count; ++index) {
            FT_Face face = nullptr;
            if (FT_New_Memory_Face(library_, bytes->data(), static_cast<FT_Long>(size), index, &face)) continue;
            FT_Select_Charmap(face, FT_ENCODING_UNICODE);
            additional_.push_back({face, bytes}); AddNames(face);
        }
        return true;
    }
private:
    static std::wstring Normalize(const std::wstring& name) {
        std::wstring result;
        for (wchar_t character : name) {
            if (character >= 0xff01 && character <= 0xff5e) character -= 0xfee0;
            if (character != L' ' && character != 0x3000) result.push_back(std::towlower(character));
        }
        return result;
    }
    void AddNames(FT_Face face) {
        if (face->family_name) facesByName_[Normalize(gstd::StringUtility::ConvertMultiToWide(face->family_name, 65001))] = face;
        for (FT_UInt index = 0; index < FT_Get_Sfnt_Name_Count(face); ++index) {
            FT_SfntName name;
            if (FT_Get_Sfnt_Name(face, index, &name) || (name.name_id != 1 && name.name_id != 16) || (name.platform_id != 0 && name.platform_id != 3)) continue;
            std::wstring decoded;
            for (FT_UInt i = 0; i + 1 < name.string_len; i += 2)
                decoded.push_back((static_cast<unsigned>(name.string[i]) << 8) | name.string[i + 1]);
            facesByName_[Normalize(decoded)] = face;
        }
    }
    struct AdditionalFace { FT_Face face; std::shared_ptr<std::vector<BYTE>> bytes; };
    std::vector<AdditionalFace> additional_;
    std::map<std::wstring, FT_Face> facesByName_;
    FT_Library library_ = nullptr;
    FT_Face face_ = nullptr;
};

FontFace& Fonts() {
    static FontFace fonts;
    return fonts;
}

void LoadGlyph(FT_Face face, unsigned code, const LOGFONT& font) {
    if (FT_Load_Char(face, code, FT_LOAD_DEFAULT))
        throw std::runtime_error("FreeType glyph loading failed");
    if (font.lfWeight >= FW_BOLD) FT_GlyphSlot_Embolden(face->glyph);
    if (font.lfItalic) FT_GlyphSlot_Oblique(face->glyph);
}

unsigned CodePoint(const wchar_t* text, int count, int& index) {
    unsigned code = static_cast<unsigned>(text[index]);
    if (code >= 0xd800 && code <= 0xdbff && index + 1 < count) {
        const unsigned low = static_cast<unsigned>(text[index + 1]);
        if (low >= 0xdc00 && low <= 0xdfff) {
            ++index;
            return 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
        }
    }
    return code;
}

int Component(DWORD color, int shift) { return (color >> shift) & 255; }

}

bool RegisterFont(const void* bytes, std::size_t size) { return Fonts().Add(bytes, size); }

SIZE MeasureText(const LOGFONT& font, const wchar_t* text, int count) {
    if (!text || count < 0) throw std::runtime_error("Invalid text measurement input");
    FT_Face face = Fonts().Select(font);
    SIZE result{0, std::abs(font.lfHeight)};
    // GDI GetTextExtentPoint32 does not apply kerning pairs. Keep Woo's per-glyph
    // advances rather than inserting shaping or a second layout engine here.
    for (int i = 0; i < count; ++i) {
        LoadGlyph(face, CodePoint(text, count, i), font);
        result.cx += static_cast<LONG>((face->glyph->advance.x + 32) / 64);
    }
    return result;
}

GlyphImage RasterizeGlyph(int code, const LOGFONT& font, int borderType,
    int borderWidth, D3DCOLOR top, D3DCOLOR bottom, D3DCOLOR border) {
    if (borderType < 0 || borderType > 2 || borderWidth < 0 || borderWidth > 64)
        throw std::runtime_error("Invalid font border");
    FT_Face face = Fonts().Select(font);
    LoadGlyph(face, static_cast<unsigned>(code), font);
    const bool mono = std::abs(font.lfHeight) <= 12;
    if (FT_Render_Glyph(face->glyph, mono ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL))
        throw std::runtime_error("FreeType glyph rasterization failed");
    FT_GlyphSlot glyph = face->glyph;
    const int pad = borderType ? borderWidth : 0;
    GlyphImage image;
    image.width = std::max(1, static_cast<int>((glyph->advance.x + 32) / 64) + 2 * pad);
    image.height = std::abs(font.lfHeight) + 2 * pad;
    image.pixels.resize(static_cast<size_t>(image.width) * image.height);
    std::vector<BYTE> coverage(image.pixels.size());
    const int ascent = static_cast<int>((face->size->metrics.ascender + 32) / 64);
    const FT_Bitmap& bitmap = glyph->bitmap;
    for (unsigned y = 0; y < bitmap.rows; ++y) {
        const BYTE* row = bitmap.pitch >= 0
            ? bitmap.buffer + y * bitmap.pitch
            : bitmap.buffer + (bitmap.rows - 1 - y) * (-bitmap.pitch);
        for (unsigned x = 0; x < bitmap.width; ++x) {
            const int dx = static_cast<int>(x) + glyph->bitmap_left + pad;
            const int dy = static_cast<int>(y) + ascent - glyph->bitmap_top + pad;
            if (dx < 0 || dx >= image.width || dy < 0 || dy >= image.height) continue;
            const BYTE alpha = bitmap.pixel_mode == FT_PIXEL_MODE_MONO
                ? ((row[x / 8] & (0x80 >> (x % 8))) ? 255 : 0) : row[x];
            coverage[dy * image.width + dx] = alpha;
        }
    }
    if (font.lfUnderline) {
        const int y = std::min(image.height - 1, pad + ascent + 1);
        for (int x = pad; x < image.width - pad; ++x) coverage[y * image.width + x] = 255;
    }
    if (font.lfStrikeOut) {
        const int y = std::min(image.height - 1, pad + ascent / 2);
        for (int x = pad; x < image.width - pad; ++x) coverage[y * image.width + x] = 255;
    }
    for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
        const float inkAlpha = coverage[y * image.width + x] / 255.0f;
        float outline = 0;
        if (borderType) for (int oy = -pad; oy <= pad; ++oy) for (int ox = -pad; ox <= pad; ++ox) {
            if (std::abs(ox) + std::abs(oy) > pad) continue;
            if (borderType == 2 && (ox > 0 || oy > 0)) continue;
            const int bx = x + ox, by = y + oy;
            if (bx >= 0 && bx < image.width && by >= 0 && by < image.height)
                outline = std::max(outline, coverage[by * image.width + bx] / 255.0f);
        }
        const float edgeAlpha = outline * (Component(border, 24) / 255.0f) * (1 - inkAlpha);
        const float alpha = inkAlpha + edgeAlpha;
        const float t = std::clamp(float(y - pad) / std::max(1, std::abs(font.lfHeight) - 1), 0.0f, 1.0f);
        DWORD color = static_cast<DWORD>(std::clamp(int(alpha * 255 + 0.5f), 0, 255)) << 24;
        for (int shift : {16, 8, 0}) {
            const float ink = Component(top, shift) * (1 - t) + Component(bottom, shift) * t;
            const int component = alpha > 0 ? std::clamp(int((ink * inkAlpha + Component(border, shift) * edgeAlpha) / alpha + 0.5f), 0, 255) : 0;
            color |= static_cast<DWORD>(component) << shift;
        }
        image.pixels[y * image.width + x] = color;
    }
    return image;
}

}
