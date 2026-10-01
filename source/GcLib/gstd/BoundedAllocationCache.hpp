#pragma once
#include <array>
#include <cstddef>
#include <new>

namespace gstd {
// A single-threaded, size-specific cache for the Web VM's short-lived value
// bodies. Destructors run BEFORE Release, so arrays/strings are never retained.
// The cache cannot grow with the length of a campaign. Overflow goes to malloc.
template<std::size_t Bytes, std::size_t Limit>
class BoundedAllocationCache {
public:
    static void* Allocate() {
        return count_ ? free_[--count_] : ::operator new(Bytes);
    }
    static void Release(void* memory) noexcept {
        if (!memory) return;
        if (count_ < Limit) free_[count_++] = memory;
        else ::operator delete(memory);
    }
    static std::size_t CachedCount() { return count_; }
    static void Purge() noexcept {
        while (count_) ::operator delete(free_[--count_]);
    }
private:
    inline static std::array<void*, Limit> free_{};
    inline static std::size_t count_ = 0;
};
}
