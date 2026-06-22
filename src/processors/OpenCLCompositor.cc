#include "OpenCLCompositor.h"

#include <cstdio>
#include <string>

namespace MugLab::OfxBase {

// Porter-Duff Over: composite a PRGB32 Blend2D overlay onto a float-RGBA src/dst pair.
// overlay[oi] layout: B G R A (PRGB32, premultiplied).
// dst / src layout: R G B A float (OFX GPU buffer).
// Y-axis is flipped between Blend2D (top-down) and OFX (bottom-up).
const char* OpenCLCompositor::kKernelSource =
    "__kernel void composite(\n"
    "    __global float*       dst,\n"
    "    __global const float* src,\n"
    "    __global const uchar* overlay,\n"
    "    int width,\n"
    "    int height,\n"
    "    int hasSrc)\n"
    "{\n"
    "    int x = get_global_id(0);\n"
    "    int y = get_global_id(1);\n"
    "    if (x >= width || y >= height) return;\n"
    "\n"
    "    int di = (y * width + x) * 4;\n"
    "    // Blend2D is top-down; OFX buffers are bottom-up — flip Y.\n"
    "    int oi = ((height - 1 - y) * width + x) * 4;\n"
    "\n"
    "    float b = overlay[oi + 0] / 255.0f;\n"
    "    float g = overlay[oi + 1] / 255.0f;\n"
    "    float r = overlay[oi + 2] / 255.0f;\n"
    "    float a = overlay[oi + 3] / 255.0f;\n"
    "    float k = 1.0f - a;\n"
    "\n"
    "    float bgR = hasSrc ? src[di + 0] : 0.0f;\n"
    "    float bgG = hasSrc ? src[di + 1] : 0.0f;\n"
    "    float bgB = hasSrc ? src[di + 2] : 0.0f;\n"
    "    float bgA = hasSrc ? src[di + 3] : 0.0f;\n"
    "\n"
    "    dst[di + 0] = r + bgR * k;\n"
    "    dst[di + 1] = g + bgG * k;\n"
    "    dst[di + 2] = b + bgB * k;\n"
    "    dst[di + 3] = a + bgA * k;\n"
    "}\n";

void OpenCLCompositor::checkError(cl_int error, const char* message) {
    if (error != CL_SUCCESS) {
        fprintf(stderr, "OpenCLCompositor: %s [%d]\n", message, error);
    }
}

auto OpenCLCompositor::getKernel(cl_command_queue commandQueue) -> cl_kernel {
    auto it = kernelMap_.find(commandQueue);
    if (it != kernelMap_.end()) { return it->second; }

    cl_int err;
    cl_context context = nullptr;
    clGetCommandQueueInfo(commandQueue, CL_QUEUE_CONTEXT, sizeof(context), &context, nullptr);

    cl_device_id device = nullptr;
    clGetCommandQueueInfo(commandQueue, CL_QUEUE_DEVICE, sizeof(device), &device, nullptr);

    const char* src = kKernelSource;
    cl_program program = clCreateProgramWithSource(context, 1, &src, nullptr, &err);
    checkError(err, "clCreateProgramWithSource");

    err = clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        size_t logSize = 0;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
        if (logSize > 1) {
            auto log = std::string(logSize, '\0');
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
            fprintf(stderr, "OpenCLCompositor build log:\n%s\n", log.c_str());
        }
    }

    cl_kernel kernel = clCreateKernel(program, "composite", &err);
    checkError(err, "clCreateKernel");
    clReleaseProgram(program);
    kernelMap_[commandQueue] = kernel;
    return kernel;
}

auto OpenCLCompositor::getOverlayBuffer(cl_command_queue commandQueue, cl_context context,
                                        size_t overlayBytes) -> cl_mem {
    auto it = overlayBufferMap_.find(commandQueue);
    if (it != overlayBufferMap_.end() && overlayBufferSizeMap_[commandQueue] == overlayBytes) {
        return it->second;
    }
    if (it != overlayBufferMap_.end()) {
        if (it->second) { clReleaseMemObject(it->second); }
        overlayBufferMap_.erase(it);
        overlayBufferSizeMap_.erase(commandQueue);
    }
    cl_int err;
    cl_mem buf = clCreateBuffer(context, CL_MEM_READ_ONLY, overlayBytes, nullptr, &err);
    if (err != CL_SUCCESS) { checkError(err, "clCreateBuffer overlay"); return nullptr; }
    overlayBufferMap_[commandQueue] = buf;
    overlayBufferSizeMap_[commandQueue] = overlayBytes;
    return buf;
}

OpenCLCompositor::~OpenCLCompositor() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [q, buf] : overlayBufferMap_) { if (buf) clReleaseMemObject(buf); }
    for (auto& [q, k]   : kernelMap_)        { if (k)   clReleaseKernel(k); }
}

auto OpenCLCompositor::composite(void* commandQueuePointer, const void* overlayPixels,
                                  void* sourceBuffer, void* destinationBuffer,
                                  int width, int height) -> bool {
    auto* commandQueue = static_cast<cl_command_queue>(commandQueuePointer);
    std::lock_guard<std::mutex> lock(mutex_);

    cl_kernel kernel = getKernel(commandQueue);
    if (kernel == nullptr) { return false; }

    cl_context context = nullptr;
    clGetCommandQueueInfo(commandQueue, CL_QUEUE_CONTEXT, sizeof(context), &context, nullptr);

    const size_t overlayBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    cl_mem overlayBuf = getOverlayBuffer(commandQueue, context, overlayBytes);
    if (overlayBuf == nullptr) { return false; }

    cl_int err = clEnqueueWriteBuffer(commandQueue, overlayBuf, CL_TRUE, 0,
                                      overlayBytes, overlayPixels, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) { checkError(err, "clEnqueueWriteBuffer overlay"); return false; }

    auto* dst    = static_cast<cl_mem>(destinationBuffer);
    auto* srcMem = sourceBuffer ? static_cast<cl_mem>(sourceBuffer) : dst;
    int   hasSrc = sourceBuffer ? 1 : 0;

    int argIdx = 0;
    err  = clSetKernelArg(kernel, argIdx++, sizeof(cl_mem), &dst);
    err |= clSetKernelArg(kernel, argIdx++, sizeof(cl_mem), &srcMem);
    err |= clSetKernelArg(kernel, argIdx++, sizeof(cl_mem), &overlayBuf);
    err |= clSetKernelArg(kernel, argIdx++, sizeof(int),    &width);
    err |= clSetKernelArg(kernel, argIdx++, sizeof(int),    &height);
    err |= clSetKernelArg(kernel, argIdx++, sizeof(int),    &hasSrc);
    if (err != CL_SUCCESS) { checkError(err, "clSetKernelArg"); return false; }

    constexpr size_t kTile = 16;
    size_t local[2]  = {kTile, kTile};
    size_t global[2] = {(static_cast<size_t>(width)  + kTile - 1) / kTile * kTile,
                        (static_cast<size_t>(height) + kTile - 1) / kTile * kTile};
    err = clEnqueueNDRangeKernel(commandQueue, kernel, 2, nullptr, global, local, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) { checkError(err, "clEnqueueNDRangeKernel"); return false; }
    clFlush(commandQueue);
    return true;
}

auto OpenCLCompositor::copyHostToDevice(void* commandQueuePointer, const void* hostPixels,
                                         void* destinationBuffer, int width, int height) -> bool {
    auto* commandQueue = static_cast<cl_command_queue>(commandQueuePointer);
    auto* dst = static_cast<cl_mem>(destinationBuffer);
    const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4 * sizeof(float);
    cl_int err = clEnqueueWriteBuffer(commandQueue, dst, CL_TRUE, 0, bytes, hostPixels, 0, nullptr, nullptr);
    return err == CL_SUCCESS;
}

}  // namespace MugLab::OfxBase
