#pragma once
#include <algorithm>
#include <filesystem>
#include <string>

namespace gstd::portable {
// Normalization is lexical. In particular, virtual archive members must not
// trigger failed stat/readdir walks every time a script assigns a texture.
inline std::string NormalizeFilePath(std::string source) {
    std::replace(source.begin(), source.end(), '\\', '/');
    auto result = std::filesystem::u8path(source).lexically_normal().generic_u8string();
    if (!source.empty() && source.back() == '/' && !result.empty() && result.back() != '/') result += '/';
    return result;
}
// The original scripts run on a case-insensitive Windows filesystem. Resolve
// existing components at the OS boundary; do not rewrite the scripts or files.
inline std::string ResolveFilePath(const std::string& source) {
    namespace fs = std::filesystem;
    auto path = fs::u8path(source).lexically_normal();
    std::error_code error;
    if (fs::exists(path, error)) return path.generic_u8string();
    fs::path result = path.is_absolute() ? path.root_path() : fs::path(".");
    auto fold = [](std::string name) {
        for (auto& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return name;
    };
    for (const auto& part : path.relative_path()) {
        if (part == "." || part.empty()) continue;
        auto next = result / part;
        error.clear();
        if (!fs::exists(next, error) && part != "..") {
            const auto wanted = fold(part.generic_u8string());
            error.clear();
            for (fs::directory_iterator it(result, error), end; !error && it != end; it.increment(error)) {
                if (fold(it->path().filename().generic_u8string()) == wanted) { next = it->path(); break; }
            }
        }
        result = std::move(next);
    }
    auto text = result.lexically_normal().generic_u8string();
    if (!source.empty() && source.back() == '/' && !text.empty() && text.back() != '/') text += '/';
    return text;
}
}
