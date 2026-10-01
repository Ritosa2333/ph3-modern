#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <utility>

namespace directx::portable {
// D3D states are small integer enums. Preserve uncommon keys, but avoid tree
// allocation and lookup for the known states on every draw and every vertex.
template<std::size_t Count>
class StateTable {
public:
    using Word = std::uint32_t;
    StateTable() = default;
    StateTable(std::initializer_list<std::pair<Word, Word>> values) {
        for (const auto& [key, value] : values) (*this)[key] = value;
    }
    Word Get(Word key) const {
        if (key < Count) return values_[key];
        const auto found = overflow_.find(key);
        return found == overflow_.end() ? 0 : found->second;
    }
    Word& operator[](Word key) {
        return key < Count ? values_[key] : overflow_[key];
    }
private:
    std::array<Word, Count> values_{};
    std::map<Word, Word> overflow_;
};
}
