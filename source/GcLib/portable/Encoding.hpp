#pragma once
#include <string>

namespace gstd::portable {
std::wstring DecodeText(const std::string& text, unsigned codePage);
std::string EncodeText(const std::wstring& text, unsigned codePage, bool* usedDefault = nullptr);
std::wstring WidePrintfFormat(const wchar_t* windowsFormat);
}
