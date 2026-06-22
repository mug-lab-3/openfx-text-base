#pragma once

#include <cstdint>

namespace MugLab::OfxBase {

enum class HandlingRole : std::uint8_t {
    None,
    Primary,
    Secondary,
    SecondaryIfOtherOwned,
    Passive
};

} // namespace MugLab::OfxBase
