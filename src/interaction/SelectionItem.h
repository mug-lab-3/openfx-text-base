#pragma once

#include <cstdint>

namespace MugLab::OfxBase {

static constexpr int kAllIndices = -1;
static constexpr int kNoSelection = -2;

enum class SelectionMode : std::uint8_t {
    Global,
    Character,
    Part
};

struct SelectionItem {
    int characterIndex_ = kAllIndices;
    int partIndex_ = kAllIndices;

    auto operator==(const SelectionItem& other) const -> bool {
        return characterIndex_ == other.characterIndex_ && partIndex_ == other.partIndex_;
    }
};

} // namespace MugLab::OfxBase
