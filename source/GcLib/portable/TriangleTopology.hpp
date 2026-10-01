#pragma once
#include <cstddef>
#include <vector>

namespace directx::portable {
// Preserve strip winding and primitive order when joining independent draws.
template<class Vertex>
void ExpandTriangles(const std::vector<Vertex>& input, std::vector<Vertex>& output,
                     std::size_t primitives, bool strip) {
    output.clear();
    output.reserve(primitives * 3);
    for (std::size_t i = 0; i < primitives; ++i) {
        const std::size_t a = strip ? i + (i & 1) : 0;
        const std::size_t b = strip ? i + 1 - (i & 1) : i + 1;
        output.push_back(input[a]);
        output.push_back(input[b]);
        output.push_back(input[i + 2]);
    }
}
}
