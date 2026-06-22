#pragma once
#include "OverlayRenderer.h"

namespace MugLab::OfxBase {

class OpenGLRenderer : public OverlayRenderer {
   public:
    explicit OpenGLRenderer(const CanvasSize& canvas);
    ~OpenGLRenderer() override = default;

    void applyColor(const Color& color) const override;
    void applyStyle(const Style& style) const override;

    void draw(PrimitiveType type, const OfxPointD* points, int count) const override;

    [[nodiscard]] auto getCanvasSize() const -> const CanvasSize& override;

   private:
    const CanvasSize canvas_;
};

}  // namespace MugLab::OfxBase
