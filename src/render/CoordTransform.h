#pragma once

#include <blend2d.h>

#include "ofxCore.h"

namespace MugLab::OfxBase::CoordTransform {

// Canvas size from the clip's Region of Definition.
// Use this — not getBounds() — as the denominator for normalized↔pixel conversions
// so positions stay correct when Resolve renders sub-tiles (bounds.x1 != 0)
// or at proxy resolution.
// Falls back to (fallbackW, fallbackH) when the RoD is empty.
inline auto canvasSizeFromRod(const OfxRectD& rod, int fallbackW, int fallbackH) -> BLSize {
    const double w = rod.x2 - rod.x1;
    const double h = rod.y2 - rod.y1;
    return {(w > 0.0) ? w : static_cast<double>(fallbackW), (h > 0.0) ? h : static_cast<double>(fallbackH)};
}

// OFX normalized position → Blend2D canvas pixel (absolute, canvas-relative).
// OFX: origin at canvas bottom-left, Y-up, (0.5, 0.5) = center.
// Blend2D: origin at canvas top-left, Y-down, pixel units.
inline auto ofxNormToBlend2D(double normX, double normY, const BLSize& canvas) -> BLPoint {
    return {normX * canvas.w, (1.0 - normY) * canvas.h};
}

// Blend2D canvas pixel → OFX normalized.
inline auto blend2DToOfxNorm(double pixX, double pixY, const BLSize& canvas) -> BLPoint {
    return {pixX / canvas.w, 1.0 - (pixY / canvas.h)};
}

// Convert a canvas-absolute Blend2D pixel position to tile-relative pixel position.
// bounds is the dstImage->getBounds() rect. Subtract its origin so the position is
// relative to the top-left corner of the current render tile.
inline auto canvasToTile(const BLPoint& canvasPx, const OfxRectI& bounds) -> BLPoint {
    return {canvasPx.x - static_cast<double>(bounds.x1), canvasPx.y - static_cast<double>(bounds.y1)};
}

// Full pipeline: OFX normalized → tile-relative Blend2D pixel.
// Equivalent to canvasToTile(ofxNormToBlend2D(...)).
inline auto ofxNormToTile(double normX, double normY, const BLSize& canvas, const OfxRectI& bounds) -> BLPoint {
    return canvasToTile(ofxNormToBlend2D(normX, normY, canvas), bounds);
}

}  // namespace MugLab::OfxBase::CoordTransform
