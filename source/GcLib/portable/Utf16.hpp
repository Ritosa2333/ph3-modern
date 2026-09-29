#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace gstd::portable {

inline std::uint16_t ReadUtf16Unit(const void* source) {
    const auto* bytes = static_cast<const unsigned char*>(source);
    return static_cast<std::uint16_t>(bytes[0] | (static_cast<unsigned>(bytes[1]) << 8));
}

// Woo's VM string model uses Windows UTF-16 code units, not native wchar_t bytes.
inline std::wstring DecodeUtf16Units(const char* bytes, std::size_t size) {
    if (size % 2) throw std::runtime_error("Odd-length UTF-16LE input");
    std::wstring result;
    result.reserve(size / 2);
    for (std::size_t i = 0; i < size; i += 2) result.push_back(ReadUtf16Unit(bytes + i));
    return result;
}

inline std::vector<char> EncodeUtf16(const std::wstring& source, bool bom = false) {
    std::vector<char> result;
    result.reserve(source.size() * 2 + (bom ? 2 : 0));
    const auto append = [&](std::uint16_t unit) {
        result.push_back(static_cast<char>(unit & 255));
        result.push_back(static_cast<char>(unit >> 8));
    };
    if (bom) append(0xfeff);
    for (wchar_t character : source) {
        std::uint32_t point = static_cast<std::uint32_t>(character);
        if (point <= 0xffff) append(static_cast<std::uint16_t>(point));
        else if (point <= 0x10ffff) {
            point -= 0x10000;
            append(static_cast<std::uint16_t>(0xd800 + (point >> 10)));
            append(static_cast<std::uint16_t>(0xdc00 + (point & 1023)));
        } else throw std::runtime_error("Invalid Unicode value");
    }
    return result;
}

}
