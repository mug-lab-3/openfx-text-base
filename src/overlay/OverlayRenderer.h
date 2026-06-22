#pragma once

#include <ofxCore.h>

namespace MugLab::OfxBase {

struct CanvasSize {
    double width_;
    double height_;
};

enum class PrimitiveType {
    Lines,
    LineStrip,
    LineLoop,
    Polygon,
    Ellipse
};

class OverlayRenderer {
   public:
    struct Color {
        float red_;
        float green_;
        float blue_;
        float alpha_;

        [[nodiscard]] constexpr auto withAlpha(float alpha) const -> Color {
            return {.red_ = red_, .green_ = green_, .blue_ = blue_, .alpha_ = alpha};
        }
    };

    struct Style {
        Color color_;
        float width_;
    };

    virtual ~OverlayRenderer() = default;

    virtual void applyColor(const Color& color) const = 0;
    virtual void applyStyle(const Style& style) const = 0;

    virtual void draw(PrimitiveType type, const OfxPointD* points, int count) const = 0;

    [[nodiscard]] virtual auto getCanvasSize() const -> const CanvasSize& = 0;
};

}  // namespace MugLab::OfxBase
