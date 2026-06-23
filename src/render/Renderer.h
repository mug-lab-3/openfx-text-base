#pragma once

#include <blend2d.h>

#include <vector>

#include "ofxsImageEffect.h"
#ifdef __APPLE__
#include "processors/MetalCompositor.h"
#else
#include "processors/OpenCLCompositor.h"
#endif

class OfxBasePlugin;

namespace MugLab::OfxBase {

class Renderer {
   public:
    Renderer() = default;
    ~Renderer() = default;

    void render(OfxBasePlugin& effect, const OFX::RenderArguments& args);

   private:
#ifdef __APPLE__
    MetalCompositor metalCompositor_;
#else
    OpenCLCompositor openclCompositor_;
#endif
    BLImage imageCache_;                  // reused across frames on the CPU path
    std::vector<uint8_t> gpuStagingBuf_;  // CPU-side PRGB32 buffer uploaded to GPU each frame
};

}  // namespace MugLab::OfxBase
