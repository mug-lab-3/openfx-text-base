#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace MugLab::OfxBase {

class FontPlatform;

class FontManager {
   public:
    struct FontInfo {
        std::string fontName_;       // global (English) name — used for secret param and internal identity
        std::string localizedName_;  // OS-locale name — used for inspector display only
        std::string fontPath_;
        uint32_t faceIndex_{0};
    };

    FontManager() = delete;
    ~FontManager() = delete;

    static void update();

    [[nodiscard]] static auto getFontList() -> const std::vector<FontInfo>&;
    [[nodiscard]] static auto getFontPath(int index) -> std::string;
    static void cleanup();

    // NOTE: Do NOT store or escape the FT_Face outside the callback.
    // The face is only valid for the duration of fn, and the mutex is held throughout.
    template <typename F>
    static auto withFace(const std::string& fontName, F&& fn) -> std::invoke_result_t<F, FT_Face> {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        return std::forward<F>(fn)(getFaceByNameLocked(fontName));
    }

   private:
    [[nodiscard]] static auto getFaceByNameLocked(const std::string& fontName) -> FT_Face;
    [[nodiscard]] static auto getFace(int choiceIndex) -> FT_Face;
    [[nodiscard]] static auto getFace(const std::string& path, uint32_t faceIndex,
                                      const std::string& fontName) -> FT_Face;
    [[nodiscard]] static auto getFontPathAndIndex(FT_Face face, std::string& outPath, uint32_t& outFaceIndex) -> bool;
    static void addFontFile(const std::string& path, uint32_t faceIndex, const std::string& defaultGlobalName,
                            const std::string& defaultLocalizedName);
    struct FontNames {
        std::string globalName_;
        std::string localizedName_;
    };
    [[nodiscard]] static auto loadFontNames(const std::string& path, uint32_t faceIndex,
                                            const std::string& fallback) -> FontNames;
    [[nodiscard]] static auto makeCacheKey(const std::string& path, uint32_t faceIndex) -> std::string;

    struct FaceCacheItem {
        FT_Face face_{nullptr};
        std::list<std::string>::iterator lruIt_;

        FaceCacheItem() = default;
        FaceCacheItem(FT_Face f, std::list<std::string>::iterator it) : face_(f), lruIt_(it) {
        }
        ~FaceCacheItem() {
            if (face_ != nullptr) {
                FT_Done_Face(face_);
            }
        }
        FaceCacheItem(const FaceCacheItem&) = delete;
        auto operator=(const FaceCacheItem&) -> FaceCacheItem& = delete;
        FaceCacheItem(FaceCacheItem&& other) noexcept : face_(other.face_), lruIt_(other.lruIt_) {
            other.face_ = nullptr;
        }
        auto operator=(FaceCacheItem&& other) noexcept -> FaceCacheItem& {
            if (this != &other) {
                if (face_ != nullptr) {
                    FT_Done_Face(face_);
                }
                face_ = other.face_;
                lruIt_ = other.lruIt_;
                other.face_ = nullptr;
            }
            return *this;
        }
    };

    static constexpr size_t kFaceCacheCapacity = 16;

    static std::recursive_mutex mutex;
    static std::unique_ptr<FontPlatform> platform;
    static std::vector<FontInfo> fonts;
    // LRU cache: lruOrder holds keys front=newest, back=oldest
    static std::list<std::string> lruOrder;
    static std::unordered_map<std::string, FaceCacheItem> fontCache;
    static FT_Library library;
};

}  // namespace MugLab::OfxBase
