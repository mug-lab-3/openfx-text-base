#pragma once

#ifndef __APPLE__

#include <CL/cl.h>
#include <map>
#include <mutex>

namespace MugLab::OfxBase {

// Composites a Blend2D PRGB32 overlay onto an OpenCL float-RGBA buffer pair.
// Kernel is compiled once per cl_command_queue and cached.
class OpenCLCompositor {
   public:
    OpenCLCompositor() = default;
    ~OpenCLCompositor();

    // commandQueuePointer: cl_command_queue cast to void* (from OFX::RenderArguments).
    // overlayPixels: tightly-packed PRGB32 host pointer (stride == width * 4).
    // sourceBuffer / destinationBuffer: cl_mem cast to void*; sourceBuffer may be nullptr.
    auto composite(void* commandQueuePointer, const void* overlayPixels,
                   void* sourceBuffer, void* destinationBuffer,
                   int width, int height) -> bool;

    // Copy CPU float RGBA pixels into an OpenCL device buffer (CPU fallback path).
    static auto copyHostToDevice(void* commandQueuePointer, const void* hostPixels,
                                 void* destinationBuffer, int width, int height) -> bool;

   private:
    static const char* kKernelSource;

    std::mutex mutex_;
    std::map<cl_command_queue, cl_kernel> kernelMap_;
    std::map<cl_command_queue, cl_mem>    overlayBufferMap_;
    std::map<cl_command_queue, size_t>    overlayBufferSizeMap_;

    static void checkError(cl_int error, const char* message);
    auto getKernel(cl_command_queue commandQueue) -> cl_kernel;
    auto getOverlayBuffer(cl_command_queue commandQueue, cl_context context,
                          size_t overlayBytes) -> cl_mem;
};

}  // namespace MugLab::OfxBase

#endif  // __APPLE__
