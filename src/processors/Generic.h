#pragma once

#include "Common.h"
#include "ofxsImageEffect.h"
#include "ofxsProcessing.h"

namespace MugLab::OfxBase {

// Copies a non-float OFX source image into a PRGB32 BLImage (with Y-flip and premultiplication).
// Used before Blend2D rasterization so the background is already in the overlay canvas.
template <typename T, int maxVal>
class OpenFxToBlend2dProcessor : public OFX::ImageProcessor {
   public:
    OpenFxToBlend2dProcessor(OFX::ImageEffect& effect, BLImage& blendImage, OFX::Image* sourceImage, OfxRectI dstBounds)
        : OFX::ImageProcessor(effect), blendImage_(blendImage), sourceImage_(sourceImage), dstBounds_(dstBounds) {
    }

    void multiThreadProcessImages(OfxRectI window) override {
        BLImageData blendData;
        blendImage_.get_data(&blendData);
        const int canvasHeight = dstBounds_.y2 - dstBounds_.y1;
        const int imageOriginY = dstBounds_.y1;

        for (int y = window.y1; y < window.y2; ++y) {
            auto* srcRow = static_cast<T*>(sourceImage_->getPixelAddress(window.x1, y));
            uint32_t* dst = getBlend2dRowPtr(blendData, canvasHeight, imageOriginY, y);

            for (int x = window.x1; x < window.x2; ++x) {
                const float r = static_cast<float>(srcRow[0]) / static_cast<float>(maxVal);
                const float g = static_cast<float>(srcRow[1]) / static_cast<float>(maxVal);
                const float b = static_cast<float>(srcRow[2]) / static_cast<float>(maxVal);
                const float a = static_cast<float>(srcRow[3]) / static_cast<float>(maxVal);
                dst[x - dstBounds_.x1] = packPreMultipliedArgb({r, g, b, a});
                srcRow += kComponentsPerPixel;
            }
        }
    }

   private:
    BLImage& blendImage_;
    OFX::Image* sourceImage_;
    OfxRectI dstBounds_;
};

// Writes the composited PRGB32 BLImage back to a non-float OFX destination buffer.
template <typename T, int maxVal>
class Blend2dToOpenFxProcessor : public OFX::ImageProcessor {
   public:
    Blend2dToOpenFxProcessor(OFX::ImageEffect& effect, BLImage& blendImage, OFX::Image* destinationImage)
        : OFX::ImageProcessor(effect), blendImage_(blendImage), destinationImage_(destinationImage) {
        dstBounds_ = destinationImage->getBounds();
    }

    void multiThreadProcessImages(OfxRectI window) override {
        BLImageData blendData;
        blendImage_.get_data(&blendData);
        const int canvasHeight = dstBounds_.y2 - dstBounds_.y1;
        const int imageOriginY = dstBounds_.y1;

        for (int y = window.y1; y < window.y2; ++y) {
            auto* dstRow = static_cast<T*>(destinationImage_->getPixelAddress(window.x1, y));
            uint32_t* blendRow = getBlend2dRowPtr(blendData, canvasHeight, imageOriginY, y);

            for (int x = window.x1; x < window.x2; ++x) {
                const uint32_t pixel = blendRow[x - dstBounds_.x1];
                dstRow[0] = static_cast<T>(unpackChannel(pixel, kRedShift) * static_cast<float>(maxVal));
                dstRow[1] = static_cast<T>(unpackChannel(pixel, kGreenShift) * static_cast<float>(maxVal));
                dstRow[2] = static_cast<T>(unpackChannel(pixel, kBlueShift) * static_cast<float>(maxVal));
                dstRow[3] = static_cast<T>(unpackChannel(pixel, kAlphaShift) * static_cast<float>(maxVal));
                dstRow += kComponentsPerPixel;
            }
        }
    }

   private:
    BLImage& blendImage_;
    OFX::Image* destinationImage_;
    OfxRectI dstBounds_;
};

}  // namespace MugLab::OfxBase
