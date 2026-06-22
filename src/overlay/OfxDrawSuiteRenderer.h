#pragma once

#include "overlay/OverlayRenderer.h"
#include "ofxDrawSuite.h"

namespace MugLab::OfxBase {

class OfxDrawSuiteRenderer : public OverlayRenderer {
   public:
    OfxDrawSuiteRenderer(const CanvasSize& canvas, OfxDrawSuiteV1* drawSuite, OfxDrawContextHandle context);
    ~OfxDrawSuiteRenderer() override = default;

    void applyColor(const Color& color) const override;
    void applyStyle(const Style& style) const override;

    void draw(PrimitiveType type, const OfxPointD* points, int count) const override;

    [[nodiscard]] auto getCanvasSize() const -> const CanvasSize& override;

   private:
    const CanvasSize canvas_;
    OfxDrawSuiteV1* drawSuite_;
    OfxDrawContextHandle context_;
};

}  // namespace MugLab::OfxBase
