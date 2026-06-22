#pragma once

#include <blend2d.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace MugLab::OfxBase {

static constexpr int   kComponentsPerPixel = 4;
static constexpr float kColorScale8Bit     = 255.0F;
static constexpr int   kAlphaShift         = 24;
static constexpr int   kRedShift           = 16;
static constexpr int   kGreenShift         =  8;
static constexpr int   kBlueShift          =  0;
static constexpr uint32_t kChannelMask     = 0xFF;

struct NormalizedRgba {
    float red_;
    float green_;
    float blue_;
    float alpha_;
};

// Returns a pointer to the PRGB32 row in a BLImage for a given OFX y coordinate.
// BLImage is top-down, OFX buffers are bottom-up — flips Y accordingly.
inline auto getBlend2dRowPtr(const BLImageData& data, int canvasHeight,
                             int imageOriginY, int y) -> uint32_t* {
    return reinterpret_cast<uint32_t*>(
        static_cast<uint8_t*>(data.pixel_data) +
        (static_cast<ptrdiff_t>(canvasHeight - 1 - (y - imageOriginY)) * data.stride));
}

inline auto packPreMultipliedArgb(const NormalizedRgba& rgba) -> uint32_t {
    auto clampAndScale = [](float val) -> uint32_t {
        return static_cast<uint32_t>(std::round(std::clamp(val, 0.0F, 1.0F) * kColorScale8Bit));
    };
    const uint32_t r = clampAndScale(rgba.red_   * rgba.alpha_);
    const uint32_t g = clampAndScale(rgba.green_ * rgba.alpha_);
    const uint32_t b = clampAndScale(rgba.blue_  * rgba.alpha_);
    const uint32_t a = clampAndScale(rgba.alpha_);
    return (a << kAlphaShift) | (r << kRedShift) | (g << kGreenShift) | (b << kBlueShift);
}

inline auto unpackChannel(uint32_t pixel, int shift) -> float {
    return static_cast<float>((pixel >> shift) & kChannelMask) / kColorScale8Bit;
}

}  // namespace MugLab::OfxBase
