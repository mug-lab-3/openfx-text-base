#include "overlay/OfxDrawSuiteRenderer.h"

#include <cmath>

#include "debugger/LogManager.h"

namespace MugLab::OfxBase {

OfxDrawSuiteRenderer::OfxDrawSuiteRenderer(const CanvasSize& canvas, OfxDrawSuiteV1* drawSuite,
                                           OfxDrawContextHandle context)
    : canvas_(canvas), drawSuite_(drawSuite), context_(context) {
}

void OfxDrawSuiteRenderer::applyColor(const Color& color) const {
    if (drawSuite_ == nullptr || context_ == nullptr) {
        return;
    }
    if (drawSuite_->setColour == nullptr) {
        LOG_ERROR("INTERACT_SUITE_ERROR", "function", "setColour", "message", "drawSuite_->setColour is NULL");
        return;
    }
    OfxRGBAColourF ofxColor = {color.red_, color.green_, color.blue_, color.alpha_};
    drawSuite_->setColour(context_, &ofxColor);
}

void OfxDrawSuiteRenderer::applyStyle(const Style& style) const {
    applyColor(style.color_);
    if (drawSuite_ == nullptr || context_ == nullptr) {
        return;
    }
    if (drawSuite_->setLineWidth == nullptr) {
        LOG_ERROR("INTERACT_SUITE_ERROR", "function", "setLineWidth", "message", "drawSuite_->setLineWidth is NULL");
        return;
    }
    if (std::isfinite(style.width_) && style.width_ > 0.0F) {
        drawSuite_->setLineWidth(context_, style.width_);
    }
}

void OfxDrawSuiteRenderer::draw(PrimitiveType type, const OfxPointD* points, int count) const {
    if (drawSuite_ == nullptr || context_ == nullptr) {
        return;
    }
    if (drawSuite_->draw == nullptr) {
        LOG_ERROR("INTERACT_SUITE_ERROR", "function", "draw", "message", "drawSuite_->draw is NULL");
        return;
    }

    OfxDrawPrimitive primitiveType = kOfxDrawPrimitiveLines;
    switch (type) {
        case PrimitiveType::Lines:
            primitiveType = kOfxDrawPrimitiveLines;
            break;
        case PrimitiveType::LineStrip:
            primitiveType = kOfxDrawPrimitiveLineStrip;
            break;
        case PrimitiveType::LineLoop:
            primitiveType = kOfxDrawPrimitiveLineLoop;
            break;
        case PrimitiveType::Polygon:
            primitiveType = kOfxDrawPrimitivePolygon;
            break;
        case PrimitiveType::Ellipse:
            primitiveType = kOfxDrawPrimitiveEllipse;
            break;
        default:
            LOG_ERROR("INTERACT_DRAW_ERROR", "message", "Unknown PrimitiveType specified");
            return;
    }

    drawSuite_->draw(context_, primitiveType, points, count);
}

auto OfxDrawSuiteRenderer::getCanvasSize() const -> const CanvasSize& {
    return canvas_;
}

}  // namespace MugLab::OfxBase
