#pragma once
#include <cstddef>
#include <cstring>
#include <map>
#include <vector>

namespace directx::portable {
// One cache per linked program. All writers to a location share the entry,
// including sampler bindings and the post-shader alpha-test override. Compare
// exact bytes, not floating-point equality: signed zero and NaN payloads matter.
class UniformValueCache {
public:
    enum class Kind { Float1, Float2, Float3, Float4, Int1, Matrix4 };

    template<class Upload>
    bool Apply(int location, Kind kind, std::size_t count,
               const void* bytes, std::size_t length, Upload upload) {
        if (location < 0 || count == 0) return false;
        auto found = entries_.find(location);
        if (found != entries_.end()) {
            const auto& old = found->second;
            if (old.kind == kind && old.count == count && old.bytes.size() == length &&
                (!length || std::memcmp(old.bytes.data(), bytes, length) == 0))
                return false;
        }
        // Array elements may also be addressed by another uniform location.
        // Do not assume location arithmetic or independent base/element names.
        // Conservatively invalidate possible aliases whenever an upload occurs.
        if (count > 1 || hasArray_) {
            for (auto entry = entries_.begin(); entry != entries_.end();) {
                if (entry->first != location && (count > 1 || entry->second.count > 1))
                    entry = entries_.erase(entry);
                else
                    ++entry;
            }
        }
        upload(); // A failed/throwing upload must not make a later call a hit.
        auto& next = entries_[location];
        next.kind = kind;
        next.count = count;
        next.bytes.resize(length);
        if (length) std::memcpy(next.bytes.data(), bytes, length);
        hasArray_ = count > 1;
        return true;
    }

    void Clear() { entries_.clear(); hasArray_ = false; }
    std::size_t Size() const { return entries_.size(); }

private:
    struct Entry {
        Kind kind = Kind::Float1;
        std::size_t count = 0;
        std::vector<unsigned char> bytes;
    };
    std::map<int, Entry> entries_;
    bool hasArray_ = false;
};
}
