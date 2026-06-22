#import <Metal/Metal.h>

#include <cstdint>
#include <iterator>
#include <mutex>
#include <thread>

#include "MetalCompositor.h"

namespace MugLab::OfxBase {

// Porter-Duff Over: composite PRGB32 overlay onto float RGBA src → dst.
// Y axis is flipped: OFX buffers are bottom-up, Blend2D is top-down.
static const char* kKernelSource =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "kernel void composite(\n"
    "    device float*       dst     [[buffer(0)]],\n"
    "    const device float* src     [[buffer(1)]],\n"
    "    const device uchar* overlay [[buffer(2)]],\n"
    "    constant int& width         [[buffer(3)]],\n"
    "    constant int& height        [[buffer(4)]],\n"
    "    constant int& hasSrc        [[buffer(5)]],\n"
    "    uint2 gid [[thread_position_in_grid]])\n"
    "{\n"
    "    int x = (int)gid.x;\n"
    "    int y = (int)gid.y;\n"
    "    if (x >= width || y >= height) return;\n"
    "    int di = (y * width + x) * 4;\n"
    // Blend2D is top-down, OFX is bottom-up → flip Y when sampling overlay.
    "    int oi = ((height - 1 - y) * width + x) * 4;\n"
    "    float b = overlay[oi + 0] / 255.0f;\n"
    "    float g = overlay[oi + 1] / 255.0f;\n"
    "    float r = overlay[oi + 2] / 255.0f;\n"
    "    float a = overlay[oi + 3] / 255.0f;\n"
    "    float k = 1.0f - a;\n"
    "    float bgR = hasSrc ? src[di + 0] : 0.0f;\n"
    "    float bgG = hasSrc ? src[di + 1] : 0.0f;\n"
    "    float bgB = hasSrc ? src[di + 2] : 0.0f;\n"
    "    float bgA = hasSrc ? src[di + 3] : 0.0f;\n"
    "    dst[di + 0] = r + bgR * k;\n"
    "    dst[di + 1] = g + bgG * k;\n"
    "    dst[di + 2] = b + bgB * k;\n"
    "    dst[di + 3] = a + bgA * k;\n"
    "}\n";

static std::mutex s_pipelineMutex;
static std::unordered_map<id<MTLCommandQueue>, id<MTLComputePipelineState>> s_pipelineCache;

auto MetalCompositor::getPipeline(void* commandQueue) -> void* {
    id<MTLCommandQueue> queue = (id<MTLCommandQueue>)commandQueue;
    id<MTLDevice> device = queue.device;

    std::lock_guard<std::mutex> lock(s_pipelineMutex);
    auto it = s_pipelineCache.find(queue);
    if (it != s_pipelineCache.end()) {
        return (__bridge void*)it->second;
    }

    NSError* err = nil;
    MTLCompileOptions* opts = [MTLCompileOptions new];
    opts.fastMathEnabled = YES;
    id<MTLLibrary> lib = [device newLibraryWithSource:@(kKernelSource) options:opts error:&err];
    [opts release];
    if (!lib) {
        fprintf(stderr, "MetalCompositor: compile failed: %s\n", err.localizedDescription.UTF8String);
        return nullptr;
    }

    id<MTLFunction> fn = [lib newFunctionWithName:@"composite"];
    [lib release];
    if (!fn) {
        fprintf(stderr, "MetalCompositor: function not found\n");
        return nullptr;
    }

    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:fn error:&err];
    [fn release];
    if (!pipeline) {
        fprintf(stderr, "MetalCompositor: pipeline failed: %s\n", err.localizedDescription.UTF8String);
        return nullptr;
    }

    s_pipelineCache[queue] = pipeline;
    return (__bridge void*)pipeline;
}

static void encodeAndCommit(id<MTLComputePipelineState> pipeline, id<MTLCommandQueue> queue,
                            id<MTLBuffer> overlayBuf, id<MTLBuffer> srcBuf, id<MTLBuffer> dstBuf,
                            int width, int height, int hasSrc) {
    id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    [enc setComputePipelineState:pipeline];
    [enc setBuffer:dstBuf     offset:0 atIndex:0];
    [enc setBuffer:srcBuf     offset:0 atIndex:1];
    [enc setBuffer:overlayBuf offset:0 atIndex:2];
    [enc setBytes:&width  length:sizeof(int) atIndex:3];
    [enc setBytes:&height length:sizeof(int) atIndex:4];
    [enc setBytes:&hasSrc length:sizeof(int) atIndex:5];
    constexpr int kTile = 16;
    MTLSize tg = MTLSizeMake(kTile, kTile, 1);
    MTLSize grid = MTLSizeMake(((size_t)width  + kTile - 1) / kTile,
                               ((size_t)height + kTile - 1) / kTile, 1);
    [enc dispatchThreadgroups:grid threadsPerThreadgroup:tg];
    [enc endEncoding];
    [cmdBuf commit];
}

MetalCompositor::~MetalCompositor() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    releaseAllBuffersLocked();
}

void MetalCompositor::releaseEntryLocked(BufferCache& entry) {
    for (int slot = 0; slot < kBuffersPerEntry; ++slot) {
        if (entry.lastCommit_[slot] != nullptr) {
            [(__bridge id<MTLCommandBuffer>)entry.lastCommit_[slot] release];
            entry.lastCommit_[slot] = nullptr;
        }
        if (entry.metalBuffer_[slot] != nullptr) {
            [(__bridge id<MTLBuffer>)entry.metalBuffer_[slot] release];
            entry.metalBuffer_[slot] = nullptr;
        }
    }
}

void MetalCompositor::releaseAllBuffersLocked() {
    for (auto& pair : bufferCaches_) {
        releaseEntryLocked(pair.second);
    }
    bufferCaches_.clear();
}

void MetalCompositor::evictLeastRecentlyUsedLocked() {
    while (bufferCaches_.size() > kMaxCacheEntries) {
        auto oldest = bufferCaches_.begin();
        for (auto it = std::next(bufferCaches_.begin()); it != bufferCaches_.end(); ++it) {
            if (it->second.lastUsed_ < oldest->second.lastUsed_) {
                oldest = it;
            }
        }
        releaseEntryLocked(oldest->second);
        bufferCaches_.erase(oldest);
    }
}

auto MetalCompositor::acquireFreeSlotLocked(BufferCache& entry) -> int {
    for (int slot = 0; slot < kBuffersPerEntry; ++slot) {
        if (entry.lastCommit_[slot] == nullptr) { return slot; }
        id<MTLCommandBuffer> commit = (__bridge id<MTLCommandBuffer>)entry.lastCommit_[slot];
        if (commit.status == MTLCommandBufferStatusCompleted ||
            commit.status == MTLCommandBufferStatusError) {
            return slot;
        }
    }
    const int slot = (entry.current_ + 1) % kBuffersPerEntry;
    [(__bridge id<MTLCommandBuffer>)entry.lastCommit_[slot] waitUntilCompleted];
    return slot;
}

auto MetalCompositor::composite(void* commandQueue, const void* overlayPixels,
                                void* sourceBuffer, void* destinationBuffer,
                                int width, int height) -> bool {
    if (commandQueue == nullptr || overlayPixels == nullptr || destinationBuffer == nullptr) {
        return false;
    }

    void* pipelinePtr = getPipeline(commandQueue);
    if (pipelinePtr == nullptr) { return false; }

    id<MTLCommandQueue> queue    = (id<MTLCommandQueue>)commandQueue;
    id<MTLDevice>       device   = queue.device;
    id<MTLComputePipelineState> pipeline = (__bridge id<MTLComputePipelineState>)pipelinePtr;

    id<MTLBuffer> overlayBuf = [device newBufferWithBytes:overlayPixels
                                                   length:(size_t)width * height * 4
                                                  options:MTLResourceStorageModeShared];
    id<MTLBuffer> dstBuf = (id<MTLBuffer>)destinationBuffer;
    id<MTLBuffer> srcBuf = sourceBuffer ? (id<MTLBuffer>)sourceBuffer : dstBuf;
    int hasSrc = sourceBuffer ? 1 : 0;

    encodeAndCommit(pipeline, queue, overlayBuf, srcBuf, dstBuf, width, height, hasSrc);
    [overlayBuf release];
    return true;
}

auto MetalCompositor::getSharedOverlayPixels(void* commandQueue, int width, int height) -> void* {
    if (commandQueue == nullptr || width <= 0 || height <= 0) { return nullptr; }

    std::thread::id threadId = std::this_thread::get_id();
    id<MTLCommandQueue> queue  = (id<MTLCommandQueue>)commandQueue;
    id<MTLDevice>       device = queue.device;

    std::lock_guard<std::mutex> lock(cacheMutex_);
    const uint64_t tick = ++accessTick_;

    auto it = bufferCaches_.find(threadId);
    if (it != bufferCaches_.end() &&
        (it->second.width_ != width || it->second.height_ != height)) {
        releaseEntryLocked(it->second);
        bufferCaches_.erase(it);
        it = bufferCaches_.end();
    }

    bool isNew = false;
    if (it == bufferCaches_.end()) {
        BufferCache newCache{};
        newCache.width_    = width;
        newCache.height_   = height;
        newCache.lastUsed_ = tick;
        it = bufferCaches_.emplace(threadId, newCache).first;
        isNew = true;
    }

    BufferCache& cache = it->second;
    cache.lastUsed_ = tick;

    const size_t bufSize = (size_t)width * height * 4;
    for (int slot = 0; slot < kBuffersPerEntry; ++slot) {
        if (cache.metalBuffer_[slot] == nullptr) {
            cache.metalBuffer_[slot] = (void*)[device newBufferWithLength:bufSize
                                                                  options:MTLResourceStorageModeShared];
        }
    }

    const int chosen = acquireFreeSlotLocked(cache);
    if (cache.lastCommit_[chosen] != nullptr) {
        [(__bridge id<MTLCommandBuffer>)cache.lastCommit_[chosen] release];
        cache.lastCommit_[chosen] = nullptr;
    }
    cache.current_ = chosen;

    if (isNew) { evictLeastRecentlyUsedLocked(); }

    return [(__bridge id<MTLBuffer>)cache.metalBuffer_[chosen] contents];
}

auto MetalCompositor::compositeShared(void* commandQueue, void* sourceBuffer,
                                      void* destinationBuffer, int width, int height) -> bool {
    if (commandQueue == nullptr || destinationBuffer == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    void* pipelinePtr = getPipeline(commandQueue);
    if (pipelinePtr == nullptr) { return false; }

    std::thread::id threadId = std::this_thread::get_id();
    id<MTLBuffer> sharedBuffer = nil;
    int slot = -1;

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = bufferCaches_.find(threadId);
        if (it != bufferCaches_.end()) {
            slot          = it->second.current_;
            sharedBuffer  = (__bridge id<MTLBuffer>)it->second.metalBuffer_[slot];
        }
    }

    if (sharedBuffer == nil) { return false; }

    id<MTLCommandQueue> queue    = (id<MTLCommandQueue>)commandQueue;
    id<MTLComputePipelineState> pipeline = (__bridge id<MTLComputePipelineState>)pipelinePtr;
    id<MTLBuffer> dstBuf = (id<MTLBuffer>)destinationBuffer;
    id<MTLBuffer> srcBuf = sourceBuffer ? (id<MTLBuffer>)sourceBuffer : dstBuf;
    int hasSrc = sourceBuffer ? 1 : 0;

    id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    [enc setComputePipelineState:pipeline];
    [enc setBuffer:dstBuf      offset:0 atIndex:0];
    [enc setBuffer:srcBuf      offset:0 atIndex:1];
    [enc setBuffer:sharedBuffer offset:0 atIndex:2];
    [enc setBytes:&width  length:sizeof(int) atIndex:3];
    [enc setBytes:&height length:sizeof(int) atIndex:4];
    [enc setBytes:&hasSrc length:sizeof(int) atIndex:5];
    constexpr int kTile = 16;
    MTLSize tg   = MTLSizeMake(kTile, kTile, 1);
    MTLSize grid = MTLSizeMake(((size_t)width  + kTile - 1) / kTile,
                               ((size_t)height + kTile - 1) / kTile, 1);
    [enc dispatchThreadgroups:grid threadsPerThreadgroup:tg];
    [enc endEncoding];
    [cmdBuf commit];

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = bufferCaches_.find(threadId);
        if (it != bufferCaches_.end() && slot >= 0 && slot < kBuffersPerEntry) {
            BufferCache& cache = it->second;
            if (cache.lastCommit_[slot] != nullptr) {
                [(__bridge id<MTLCommandBuffer>)cache.lastCommit_[slot] release];
            }
            cache.lastCommit_[slot] = (void*)[cmdBuf retain];
        }
    }
    return true;
}

}  // namespace MugLab::OfxBase
