#include "Renderer.h"

#include <blend2d.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <hb-ft.h>
#include <hb.h>

#include <cstdint>
#include <format>
#include <sstream>

#include "font/FontManager.h"
#include "main.h"
#include "params/ParamIds.h"
#include "processors/Common.h"
#include "processors/Generic.h"
#include "render/CoordTransform.h"

namespace MugLab::OfxBase {

// ==============================================================================
// FreeType outline → BLPath decomposition
// ==============================================================================
struct OutlineCtx {
    BLPath* path_;
    double scale_;
    double penX_;
    double penY_;
};

static int ftMoveTo(const FT_Vector* to, void* user) {
    auto* c = static_cast<OutlineCtx*>(user);
    c->path_->move_to(c->penX_ + to->x * c->scale_, c->penY_ - to->y * c->scale_);
    return 0;
}
static int ftLineTo(const FT_Vector* to, void* user) {
    auto* c = static_cast<OutlineCtx*>(user);
    c->path_->line_to(c->penX_ + to->x * c->scale_, c->penY_ - to->y * c->scale_);
    return 0;
}
static int ftConicTo(const FT_Vector* ctrl, const FT_Vector* to, void* user) {
    auto* c = static_cast<OutlineCtx*>(user);
    c->path_->quad_to(c->penX_ + ctrl->x * c->scale_, c->penY_ - ctrl->y * c->scale_, c->penX_ + to->x * c->scale_,
                      c->penY_ - to->y * c->scale_);
    return 0;
}
static int ftCubicTo(const FT_Vector* c1, const FT_Vector* c2, const FT_Vector* to, void* user) {
    auto* c = static_cast<OutlineCtx*>(user);
    c->path_->cubic_to(c->penX_ + c1->x * c->scale_, c->penY_ - c1->y * c->scale_, c->penX_ + c2->x * c->scale_,
                       c->penY_ - c2->y * c->scale_, c->penX_ + to->x * c->scale_, c->penY_ - to->y * c->scale_);
    return 0;
}
static const FT_Outline_Funcs kOutlineFuncs = {
    .move_to = ftMoveTo,
    .line_to = ftLineTo,
    .conic_to = ftConicTo,
    .cubic_to = ftCubicTo,
    .shift = 0,
    .delta = 0,
};

// ==============================================================================
// drawText: FontManager → FreeType → HarfBuzz → BLPath → BLContext
// Renders into whatever BLImage is passed in (caller owns the backing memory).
// ==============================================================================
// Returns the total x-advance of a shaped line in pixels.
static auto measureLineAdvance(FT_Face face, hb_font_t* hbFont, const std::string& line, double fontSize) -> double {
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, line.c_str(), -1, 0, -1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(hbFont, buf, nullptr, 0);

    unsigned int glyphCount = 0;
    hb_glyph_position_t* poses = hb_buffer_get_glyph_positions(buf, &glyphCount);

    const double scale = fontSize / static_cast<double>(face->units_per_EM);
    double advance = 0.0;
    for (unsigned int i = 0; i < glyphCount; ++i) {
        advance += static_cast<double>(poses[i].x_advance) * scale;
    }

    hb_buffer_destroy(buf);
    return advance;
}

static void drawTextLine(BLContext& ctx, FT_Face face, hb_font_t* hbFont, const std::string& line, double fontSize,
                         double originX, double originY) {
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, line.c_str(), -1, 0, -1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(hbFont, buf, nullptr, 0);

    unsigned int glyphCount = 0;
    hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buf, &glyphCount);
    hb_glyph_position_t* poses = hb_buffer_get_glyph_positions(buf, &glyphCount);

    const double scale = fontSize / static_cast<double>(face->units_per_EM);
    double penX = originX;
    double penY = originY;

    for (unsigned int i = 0; i < glyphCount; ++i) {
        if (FT_Load_Glyph(face, infos[i].codepoint, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING) != 0) {
            penX += static_cast<double>(poses[i].x_advance) * scale;
            continue;
        }

        const double gx = penX + static_cast<double>(poses[i].x_offset) * scale;
        const double gy = penY - static_cast<double>(poses[i].y_offset) * scale;

        BLPath glyphPath;
        OutlineCtx oc{&glyphPath, scale, gx, gy};
        FT_Outline_Decompose(&face->glyph->outline, &kOutlineFuncs, &oc);
        glyphPath.close();
        ctx.fill_path(glyphPath);

        penX += static_cast<double>(poses[i].x_advance) * scale;
        penY -= static_cast<double>(poses[i].y_advance) * scale;
    }

    hb_buffer_destroy(buf);
}

// Renders multi-line text with `centerX/centerY` as the center of the text bounding box.
// BB width  = widest line advance
// BB height = lineCount * lineHeight  (lineHeight = fontSize * 1.2)
// The baseline of the first line is placed at centerY - bbHeight/2 + fontSize.
static void drawText(BLImage& target, const std::string& text, double fontSize, double centerX, double centerY,
                     const std::string& fontName) {
    const auto& fonts = FontManager::getFontList();
    if (fonts.empty()) {
        return;
    }

    const std::string& resolvedFont = fontName.empty() ? fonts[0].fontName_ : fontName;
    FontManager::withFace(resolvedFont, [&](FT_Face face) {
        if (face == nullptr) {
            return;
        }

        hb_font_t* hbFont = hb_ft_font_create(face, nullptr);
        hb_font_set_scale(hbFont, face->units_per_EM, face->units_per_EM);

        const double lineHeight = fontSize * 1.2;

        // Split lines and measure BB.
        std::vector<std::string> lines;
        {
            std::istringstream stream(text);
            std::string ln;
            while (std::getline(stream, ln)) {
                lines.push_back(std::move(ln));
            }
        }

        double maxAdvance = 0.0;
        for (const auto& ln : lines) {
            if (!ln.empty()) {
                maxAdvance = std::max(maxAdvance, measureLineAdvance(face, hbFont, ln, fontSize));
            }
        }

        const double bbWidth = maxAdvance;
        const double bbHeight = static_cast<double>(lines.size()) * lineHeight;

        // Top-left of BB → first baseline = top + fontSize (ascender approximation).
        const double startX = centerX - bbWidth * 0.5;
        const double startY = centerY - bbHeight * 0.5 + fontSize;

        BLContext ctx(target);
        ctx.set_fill_style(BLRgba32(0xFFFFFFFF));

        double y = startY;
        for (const auto& ln : lines) {
            if (!ln.empty()) {
                drawTextLine(ctx, face, hbFont, ln, fontSize, startX, y);
            }
            y += lineHeight;
        }

        ctx.end();
        hb_font_destroy(hbFont);
    });
}

// ==============================================================================
// compositeOverlayScalar
// Porter-Duff Over: PRGB32 BLImage overlay (top-down) → float RGBA dst (bottom-up).
// Used only in the CPU float path; 8/16-bit paths use Blend2dToOpenFxProcessor.
// ==============================================================================
static void compositeOverlayScalar(const BLImageData& overlay, const float* src, float* dst, int w, int h) {
    for (int y = 0; y < h; ++y) {
        const uint32_t* row =
            getBlend2dRowPtr(overlay, h, 0, y);  // imageOriginY=0 because overlay is sized exactly w×h
        const float* srcRow = src ? (src + static_cast<ptrdiff_t>(y) * w * 4) : nullptr;
        float* dstRow = dst + static_cast<ptrdiff_t>(y) * w * 4;

        for (int x = 0; x < w; ++x) {
            const uint32_t p = row[x];
            const float r = unpackChannel(p, kRedShift);
            const float g = unpackChannel(p, kGreenShift);
            const float b = unpackChannel(p, kBlueShift);
            const float a = unpackChannel(p, kAlphaShift);
            const float k = 1.0F - a;

            if (srcRow != nullptr) {
                dstRow[0] = r + (srcRow[0] * k);
                dstRow[1] = g + (srcRow[1] * k);
                dstRow[2] = b + (srcRow[2] * k);
                dstRow[3] = a + (srcRow[3] * k);
                srcRow += 4;
            } else {
                dstRow[0] = r;
                dstRow[1] = g;
                dstRow[2] = b;
                dstRow[3] = a;
            }
            dstRow += 4;
        }
    }
}

// ==============================================================================
// buildParamDump: full parameter value dump as a multi-line string
// ==============================================================================
static auto buildParamDump(OfxBasePlugin& effect, double time, const std::string& renderPath) -> std::string {
    const auto& p = effect.params_;
    std::ostringstream ss;

    ss << "[Render] " << renderPath << "\n";
    ss << "[Page] " << p.getChoiceOption(ParamIds::kPageSelect, time) << "\n";

    ss << "[Text]\n";
    ss << "  text     : " << p.getString(ParamIds::kTextContent, time) << "\n";
    ss << "  font     : " << p.getChoiceOption(ParamIds::kFontName, time) << "\n";
    ss << "  fontSize : " << std::format("{:.1f}", p.getDouble(ParamIds::kFontSize, time)) << "\n";

    ss << "[Layout]\n";
    const auto pos = p.getDouble2D(ParamIds::kPosition, time);
    ss << "  position : " << std::format("{:.1f}, {:.1f}", pos.x_, pos.y_) << "\n";
    ss << "  size     : " << std::format("{:.2f}", p.getDouble(ParamIds::kSize, time)) << "\n";

    ss << "[Style]\n";
    ss << "  opacity        : " << std::format("{:.2f}", p.getDouble(ParamIds::kOpacity, time)) << "\n";
    const bool useColor = p.getBool(ParamIds::kUseCustomColor, time);
    ss << "  useCustomColor : " << (useColor ? "true" : "false") << "\n";
    if (useColor) {
        const auto col = p.getRGBA(ParamIds::kColor, time);
        ss << "  color          : " << std::format("{:.2f}, {:.2f}, {:.2f}, {:.2f}", col.r_, col.g_, col.b_, col.a_)
           << "\n";
    }

    for (int i = 0; i < ParamIds::kShadowSlotCount; ++i) {
        const std::string s = std::to_string(i);
        const bool enabled = p.getBool(std::string(ParamIds::kShadowEnabledPrefix) + s, time);
        ss << "[Shadow " << (i + 1) << "]\n";
        ss << "  enabled : " << (enabled ? "true" : "false") << "\n";
        if (enabled) {
            const auto off = p.getDouble2D(std::string(ParamIds::kShadowOffsetPrefix) + s, time);
            ss << "  offset  : " << std::format("{:.1f}, {:.1f}", off.x_, off.y_) << "\n";
            const auto col = p.getRGBA(std::string(ParamIds::kShadowColorPrefix) + s, time);
            ss << "  color   : " << std::format("{:.2f}, {:.2f}, {:.2f}, {:.2f}", col.r_, col.g_, col.b_, col.a_)
               << "\n";
        }
    }

    return ss.str();
}

// ==============================================================================
// Renderer::render
//
// GPU path (OpenCL):
//   DaVinci Resolve passes float32 RGBA cl_mem buffers on the GPU path.
//   The overlay is rendered into a host PRGB32 staging buffer and composited
//   via OpenCLCompositor, which runs a Porter-Duff Over kernel on the GPU.
//   NOTE: if Resolve ever passes 8/16-bit buffers on the GPU path the composite
//   kernel will mis-interpret the memory and crash — there is no safe CPU
//   fallback for that case because the buffer lives on the device.
//
// CPU path:
//   Supports float32, uint16 (16-bit), and uint8 (8-bit) output buffers.
//   8/16-bit src → pre-copied into PRGB32 imageCache_ via OpenFxToBlend2dProcessor
//   float src    → composited after rasterization via compositeOverlayScalar
//   8/16-bit dst → written back via Blend2dToOpenFxProcessor
// ==============================================================================
void Renderer::render(OfxBasePlugin& effect, const OFX::RenderArguments& args) {
    const std::string kFontName = effect.params_.getChoiceOption(ParamIds::kFontName, args.time);

    // Determine render path from args before any early-out so the dump is always accurate.
    std::string renderPath = "CPU";
    if (args.isEnabledMetalRender && args.pMetalCmdQ != nullptr) {
        renderPath = "Metal";
    } else if (args.isEnabledCudaRender && args.pCudaStream != nullptr) {
        renderPath = "CUDA";
    } else if (args.isEnabledOpenCLRender && args.pOpenCLCmdQ != nullptr) {
        renderPath = "OpenCL";
    }

    const std::string kText = buildParamDump(effect, args.time, renderPath);

    auto dstImageOwner = std::unique_ptr<OFX::Image>(effect.dstClip_->fetchImage(args.time));
    auto* dstImage = dstImageOwner.get();
    if (dstImage == nullptr) {
        return;
    }

    const OfxRectI bounds = dstImage->getBounds();
    const int w = bounds.x2 - bounds.x1;
    const int h = bounds.y2 - bounds.y1;
    if (w <= 0 || h <= 0) {
        return;
    }

    const auto rod = effect.dstClip_->getRegionOfDefinition(args.time);
    const auto canvas = CoordTransform::canvasSizeFromRod(rod, w, h);

    const auto kPos = effect.params_.getDouble2D(ParamIds::kPosition, args.time, {.x_ = 0.5, .y_ = 0.5});
    const auto kOrigin = CoordTransform::ofxNormToTile(kPos.x_, kPos.y_, canvas, bounds);
    const double kOriginX = kOrigin.x;
    const double kOriginY = kOrigin.y;
    const double kFontSizeParam = effect.params_.getDouble(ParamIds::kFontSize, args.time, kDefaultFontSize);
    const double kFontSize = kFontSizeParam * (canvas.h / 1080.0);

#ifdef __APPLE__
    // --- GPU (Metal) path ---
    // Uses zero-copy shared memory: getSharedOverlayPixels() returns a CPU-writable
    // MTLBuffer pointer; compositeShared() submits the GPU work without an upload copy.
    if (args.isEnabledMetalRender && args.pMetalCmdQ != nullptr) {
        void* sharedPixels = metalCompositor_.getSharedOverlayPixels(args.pMetalCmdQ, w, h);
        if (sharedPixels != nullptr) {
            BLImage overlay;
            overlay.create_from_data(w, h, BL_FORMAT_PRGB32, sharedPixels, static_cast<intptr_t>(w) * 4);
            {
                BLContext c(overlay);
                c.clear_all();
                c.end();
            }
            drawText(overlay, kText, kFontSize, kOriginX, kOriginY, kFontName);
            overlay.reset();

            std::unique_ptr<OFX::Image> srcImg;
            if (effect.srcClip_ != nullptr && effect.srcClip_->isConnected()) {
                srcImg.reset(effect.srcClip_->fetchImage(args.time));
            }
            void* srcBuf = srcImg ? srcImg->getPixelData() : nullptr;
            void* dstBuf = dstImage->getPixelData();

            if (metalCompositor_.compositeShared(args.pMetalCmdQ, srcBuf, dstBuf, w, h)) {
                return;
            }
        }
        // Shared memory unavailable — fall through to CPU path.
    }
#else
    // --- GPU (OpenCL) path ---
    // Assumes float32 RGBA cl_mem buffers. 8/16-bit GPU buffers are not supported:
    // the kernel would misinterpret the memory layout and crash. DaVinci Resolve
    // always uses float32 for GPU-accelerated effects, so this is safe in practice.
    if (args.pOpenCLCmdQ != nullptr) {
        const size_t packedSize = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
        if (gpuStagingBuf_.size() != packedSize) {
            gpuStagingBuf_.assign(packedSize, 0);
        }

        BLImage overlay;
        overlay.create_from_data(w, h, BL_FORMAT_PRGB32, gpuStagingBuf_.data(), static_cast<intptr_t>(w) * 4);
        {
            BLContext c(overlay);
            c.clear_all();
            c.end();
        }
        drawText(overlay, kText, kFontSize, kOriginX, kOriginY, kFontName);
        overlay.reset();

        std::unique_ptr<OFX::Image> srcImg;
        if (effect.srcClip_ != nullptr && effect.srcClip_->isConnected()) {
            srcImg.reset(effect.srcClip_->fetchImage(args.time));
        }
        void* srcBuf = srcImg ? srcImg->getPixelData() : nullptr;
        void* dstBuf = dstImage->getPixelData();

        if (openclCompositor_.composite(args.pOpenCLCmdQ, gpuStagingBuf_.data(), srcBuf, dstBuf, w, h)) {
            return;
        }
        // OpenCL composite failed. CPU fallback below handles host-accessible
        // float buffers only. If the buffer is device-only this will crash —
        // but that should not happen when pOpenCLCmdQ is set on Resolve.
    }
#endif

    // --- CPU path ---
    // Fetch src (optional)
    std::unique_ptr<OFX::Image> srcImg;
    if (effect.srcClip_ != nullptr && effect.srcClip_->isConnected()) {
        srcImg.reset(effect.srcClip_->fetchImage(args.time));
    }

    // Allocate or reuse the PRGB32 imageCache_
    if (imageCache_.width() != w || imageCache_.height() != h) {
        imageCache_.create(w, h, BL_FORMAT_PRGB32);
    }

    // For 8/16-bit src: blit src into imageCache_ before Blend2D opens a BLContext on it.
    // (BLContext holds an exclusive lock on the image while open.)
    bool srcPrecopied = false;
    if (srcImg && srcImg->getPixelDepth() != OFX::eBitDepthFloat) {
        if (srcImg->getPixelDepth() == OFX::eBitDepthUShort) {
            OpenFxToBlend2dProcessor<uint16_t, 65535> proc(effect, imageCache_, srcImg.get(), bounds);
            proc.setRenderWindow(args.renderWindow);
            proc.process();
        } else {
            OpenFxToBlend2dProcessor<uint8_t, 255> proc(effect, imageCache_, srcImg.get(), bounds);
            proc.setRenderWindow(args.renderWindow);
            proc.process();
        }
        srcPrecopied = true;
    }

    // Rasterize text into imageCache_ (clears to transparent first when src is float or absent)
    if (!srcPrecopied) {
        BLContext clearCtx(imageCache_);
        clearCtx.clear_all();
        clearCtx.end();
    }
    drawText(imageCache_, kText, kFontSize, kOriginX, kOriginY, kFontName);

    // Write result to destination
    const auto dstDepth = dstImage->getPixelDepth();

    if (dstDepth == OFX::eBitDepthFloat) {
        const float* srcPixels = (srcImg && !srcPrecopied)
                                     ? static_cast<const float*>(srcImg->getPixelAddress(bounds.x1, bounds.y1))
                                     : nullptr;
        auto* dstPixels = static_cast<float*>(dstImage->getPixelAddress(bounds.x1, bounds.y1));

        BLImageData cacheData{};
        imageCache_.get_data(&cacheData);
        compositeOverlayScalar(cacheData, srcPixels, dstPixels, w, h);

    } else if (dstDepth == OFX::eBitDepthUShort) {
        Blend2dToOpenFxProcessor<uint16_t, 65535> proc(effect, imageCache_, dstImage);
        proc.setRenderWindow(args.renderWindow);
        proc.process();

    } else {
        Blend2dToOpenFxProcessor<uint8_t, 255> proc(effect, imageCache_, dstImage);
        proc.setRenderWindow(args.renderWindow);
        proc.process();
    }
}

}  // namespace MugLab::OfxBase
