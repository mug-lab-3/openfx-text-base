#pragma once

#ifdef __APPLE__

#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace MugLab::OfxBase {

// Composites a Blend2D PRGB32 overlay onto a Metal float-RGBA buffer pair.
class MetalCompositor {
   public:
    MetalCompositor() = default;
    ~MetalCompositor();

    // Upload overlayPixels (PRGB32, CPU) and composite onto destinationBuffer (id<MTLBuffer>, float RGBA).
    // sourceBuffer may be nullptr when the source clip is unconnected (generator context).
    auto composite(void* commandQueue, const void* overlayPixels, void* sourceBuffer, void* destinationBuffer,
                   int width, int height) -> bool;

    // Returns shared CPU/GPU memory for zero-copy overlay rendering.
    // The caller writes the PRGB32 overlay into this pointer, then calls compositeShared().
    auto getSharedOverlayPixels(void* commandQueue, int width, int height) -> void*;

    // Composites using the shared buffer previously returned by getSharedOverlayPixels().
    auto compositeShared(void* commandQueue, void* sourceBuffer, void* destinationBuffer, int width,
                         int height) -> bool;

   private:
    static constexpr int kBuffersPerEntry = 2;
    static constexpr size_t kMaxCacheEntries = 6;

    struct BufferCache {
        void* metalBuffer_[kBuffersPerEntry] = {nullptr, nullptr};
        void* lastCommit_[kBuffersPerEntry] = {nullptr, nullptr};
        int width_ = 0;
        int height_ = 0;
        int current_ = 0;
        uint64_t lastUsed_ = 0;
    };

    std::unordered_map<std::thread::id, BufferCache> bufferCaches_;
    std::mutex cacheMutex_;
    uint64_t accessTick_ = 0;

    void releaseEntryLocked(BufferCache& entry);
    void releaseAllBuffersLocked();
    void evictLeastRecentlyUsedLocked();
    static auto acquireFreeSlotLocked(BufferCache& entry) -> int;

    // Returns (or compiles and caches) the compute pipeline for the given command queue.
    static auto getPipeline(void* commandQueue) -> void*;
};

}  // namespace MugLab::OfxBase

#endif  // __APPLE__
