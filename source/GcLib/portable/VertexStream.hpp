#pragma once
#include <cstddef>

namespace directx::portable {
// Pure allocation plan for one bounded streaming VBO. Orphan before reuse so
// no in-flight draw sees overwritten vertices. Oversized draws remain intact.
template<std::size_t Capacity>
class VertexStream {
public:
    struct Reservation { std::size_t first; bool orphan; bool direct; };
    void BeginFrame() { cursor_ = Capacity; }
    Reservation Reserve(std::size_t count) {
        if (count > Capacity) { cursor_ = Capacity; return {0, false, true}; }
        const bool orphan = cursor_ == Capacity || count > Capacity - cursor_;
        if (orphan) cursor_ = 0;
        const auto first = cursor_; cursor_ += count;
        return {first, orphan, false};
    }
private:
    std::size_t cursor_ = Capacity;
};
}
