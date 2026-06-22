#include "OpenGLRenderer.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace MugLab::OfxBase {

OpenGLRenderer::OpenGLRenderer(const CanvasSize& canvas) : canvas_(canvas) {
}

void OpenGLRenderer::applyColor(const Color& color) const {
    glColor4f(color.red_, color.green_, color.blue_, color.alpha_);
}

void OpenGLRenderer::applyStyle(const Style& style) const {
    applyColor(style.color_);
    if (style.width_ > 0.0F) {
        glLineWidth(style.width_);
    }
}

void OpenGLRenderer::draw(PrimitiveType type, const OfxPointD* points, int count) const {
    if (points == nullptr || count <= 0) {
        return;
    }

    if (type == PrimitiveType::Ellipse) {
        if (count < 2) {
            return;
        }
        const double centerX = (points[0].x + points[1].x) / 2.0;
        const double centerY = (points[0].y + points[1].y) / 2.0;
        const double radiusX = std::abs(points[1].x - points[0].x) / 2.0;
        const double radiusY = std::abs(points[1].y - points[0].y) / 2.0;

        constexpr int segmentCount = 64;
        glBegin(GL_LINE_LOOP);
        for (int index = 0; index < segmentCount; ++index) {
            const double theta = 2.0 * M_PI * static_cast<double>(index) / segmentCount;
            glVertex2d(centerX + radiusX * std::cos(theta), centerY + radiusY * std::sin(theta));
        }
        glEnd();
    } else {
        GLenum openGlMode = GL_LINES;
        switch (type) {
            case PrimitiveType::Lines:
                openGlMode = GL_LINES;
                break;
            case PrimitiveType::LineStrip:
                openGlMode = GL_LINE_STRIP;
                break;
            case PrimitiveType::LineLoop:
                openGlMode = GL_LINE_LOOP;
                break;
            case PrimitiveType::Polygon:
                openGlMode = GL_POLYGON;
                break;
            default:
                openGlMode = GL_LINES;
                break;
        }

        glBegin(openGlMode);
        for (int index = 0; index < count; ++index) {
            glVertex2d(points[index].x, points[index].y);
        }
        glEnd();
    }
}

auto OpenGLRenderer::getCanvasSize() const -> const CanvasSize& {
    return canvas_;
}

}  // namespace MugLab::OfxBase
