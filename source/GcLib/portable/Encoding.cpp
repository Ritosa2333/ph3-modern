#include "Encoding.hpp"
#include "Win32Compat.hpp"
#include <algorithm>
#include <stdexcept>
#include "Cp932Table.inc"

namespace gstd::portable {
namespace {
template<std::size_t N>
unsigned Lookup(const Cp932Pair (&table)[N], unsigned source, unsigned fallback) {
    const auto* end = table + N;
    const auto* found = std::lower_bound(table, end, source,
        [](const Cp932Pair& item, unsigned code) { return item.source < code; });
    return found != end && found->source == source ? found->destination : fallback;
}
void AppendWide(std::wstring& text, unsigned point) {
    if (point <= 0xffff) text.push_back(static_cast<wchar_t>(point));
    else { point -= 0x10000; text.push_back(0xd800 + (point >> 10)); text.push_back(0xdc00 + (point & 1023)); }
}
}

std::wstring DecodeText(const std::string& text, unsigned codePage) {
    std::wstring result;
    result.reserve(text.size());
    if (codePage == 932 || codePage == CP_ACP) {
        for (std::size_t i = 0; i < text.size(); ++i) {
            unsigned code = static_cast<BYTE>(text[i]);
            if (IsDBCSLeadByte(static_cast<BYTE>(code))) {
                if (i + 1 >= text.size()) { result.push_back(0xfffd); break; }
                code = (code << 8) | static_cast<BYTE>(text[++i]);
            }
            result.push_back(static_cast<wchar_t>(Lookup(Cp932Decode, code, 0xfffd)));
        }
    } else if (codePage == CP_UTF8) {
        for (std::size_t i = 0; i < text.size();) {
            unsigned first = static_cast<BYTE>(text[i++]);
            if (first < 0x80) { result.push_back(first); continue; }
            unsigned count = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : 0;
            if (!count || count > text.size() - i) { result.push_back(0xfffd); continue; }
            unsigned point = first & ((1u << (6 - count)) - 1);
            bool valid = true;
            for (unsigned j = 0; j < count; ++j) {
                unsigned next = static_cast<BYTE>(text[i + j]);
                if ((next & 0xc0) != 0x80) { valid = false; break; }
                point = (point << 6) | (next & 63);
            }
            unsigned minimum = count == 1 ? 0x80 : count == 2 ? 0x800 : 0x10000;
            if (!valid || point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) result.push_back(0xfffd);
            else { i += count; AppendWide(result, point); }
        }
    } else throw std::runtime_error("Unsupported text code page " + std::to_string(codePage));
    return result;
}

std::string EncodeText(const std::wstring& text, unsigned codePage, bool* usedDefault) {
    std::string result;
    result.reserve(codePage == CP_UTF8 ? text.size() * 2 : text.size());
    if (usedDefault) *usedDefault = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned point = static_cast<unsigned>(text[i]);
        if (point >= 0xd800 && point <= 0xdbff && i + 1 < text.size()) {
            unsigned low = static_cast<unsigned>(text[i + 1]);
            if (low >= 0xdc00 && low <= 0xdfff) { point = 0x10000 + ((point - 0xd800) << 10) + low - 0xdc00; ++i; }
        }
        if (codePage == 932 || codePage == CP_ACP) {
            // ASCII and JIS X 0201 half-width katakana are direct CP932 ranges.
            // Common-data names and keys usually stay on this fast path.
            unsigned code;
            if (point <= 0x7f) code = point;
            else if (point >= 0xff61 && point <= 0xff9f) code = 0xa1 + (point - 0xff61);
            else code = Lookup(Cp932Encode, point, 0xffffffffu);
            if (code == 0xffffffffu) { code = '?'; if (usedDefault) *usedDefault = true; }
            if (code > 255) result.push_back(static_cast<char>(code >> 8));
            result.push_back(static_cast<char>(code));
        } else if (codePage == CP_UTF8) {
            if (point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) point = 0xfffd;
            if (point < 0x80) result.push_back(static_cast<char>(point));
            else if (point < 0x800) { result.push_back(static_cast<char>(0xc0 | (point >> 6))); result.push_back(static_cast<char>(0x80 | (point & 63))); }
            else if (point < 0x10000) { result.push_back(static_cast<char>(0xe0 | (point >> 12))); result.push_back(static_cast<char>(0x80 | ((point >> 6) & 63))); result.push_back(static_cast<char>(0x80 | (point & 63))); }
            else { result.push_back(static_cast<char>(0xf0 | (point >> 18))); result.push_back(static_cast<char>(0x80 | ((point >> 12) & 63))); result.push_back(static_cast<char>(0x80 | ((point >> 6) & 63))); result.push_back(static_cast<char>(0x80 | (point & 63))); }
        } else throw std::runtime_error("Unsupported text code page " + std::to_string(codePage));
    }
    return result;
}

std::wstring WidePrintfFormat(const wchar_t* source) {
    // MSVCRT wide printf's %s/%c consume wide values; ISO printf requires %ls/%lc.
    // MSVCRT's 64-bit integer modifier is I64; musl/ISO printf uses ll.
    std::wstring result;
    for (std::size_t i = 0; source[i]; ++i) {
        result.push_back(source[i]);
        if (source[i] != L'%') continue;
        if (source[i + 1] == L'%') { result.push_back(source[++i]); continue; }
        bool length = false;
        for (++i; source[i]; ++i) {
            wchar_t c = source[i];
            if (c == L'I' && source[i + 1] == L'6' && source[i + 2] == L'4') {
                result.push_back(L'l');
                result.push_back(L'l');
                i += 2;
                length = true;
                continue;
            }
            if (c == L'l' || c == L'h' || c == L'j' || c == L'z' || c == L't' || c == L'L') length = true;
            if ((c == L's' || c == L'c') && !length) result.push_back(L'l');
            if (c == L'S') c = L's';
            if (c == L'C') c = L'c';
            result.push_back(c);
            if (std::wcschr(L"diuoxXfFeEgGaAcCsSpn", source[i])) break;
        }
        if (!source[i]) break;
    }
    return result;
}
}

extern "C" int MultiByteToWideChar(UINT codePage, DWORD, LPCSTR source, int count, LPWSTR destination, int capacity) {
    if (!source || count == 0 || count < -1 || capacity < 0) return 0;
    std::string text(source, count < 0 ? std::strlen(source) + 1 : static_cast<std::size_t>(count));
    auto converted = gstd::portable::DecodeText(text, codePage);
    if (!capacity) return static_cast<int>(converted.size());
    if (!destination || converted.size() > static_cast<std::size_t>(capacity)) return 0;
    std::copy(converted.begin(), converted.end(), destination);
    return static_cast<int>(converted.size());
}
extern "C" int WideCharToMultiByte(UINT codePage, DWORD, LPCWSTR source, int count, LPSTR destination, int capacity, LPCSTR, BOOL* usedDefault) {
    if (!source || count == 0 || count < -1 || capacity < 0) return 0;
    std::wstring text(source, count < 0 ? std::wcslen(source) + 1 : static_cast<std::size_t>(count));
    bool fallback = false;
    auto converted = gstd::portable::EncodeText(text, codePage, &fallback);
    if (usedDefault) *usedDefault = fallback;
    if (!capacity) return static_cast<int>(converted.size());
    if (!destination || converted.size() > static_cast<std::size_t>(capacity)) return 0;
    std::copy(converted.begin(), converted.end(), destination);
    return static_cast<int>(converted.size());
}
