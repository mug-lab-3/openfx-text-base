#include "FontManager.h"

#include <blend2d.h>
#include <ft2build.h>
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_MULTIPLE_MASTERS_H
#include <utf8/cpp17.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <mutex>

#include "../debugger/LogManager.h"
#include "FontPlatform.h"
#ifdef _WIN32
#include "Win32FontPlatform.h"
#elif defined(__APPLE__)
#include "AppleFontPlatform.h"
#else
#include "LinuxFontPlatform.h"
#endif

namespace MugLab::OfxBase {

namespace {
auto decodeUtf16BE(const FT_Byte* data, FT_UInt len) -> std::string;
auto getSfntNameById(FT_Face face, FT_UShort nameIdentifier, FT_UShort languageId) -> std::string;
}  // namespace

std::recursive_mutex FontManager::mutex;
std::unique_ptr<FontPlatform> FontManager::platform = nullptr;
std::vector<FontManager::FontInfo> FontManager::fonts;
std::list<std::string> FontManager::lruOrder;
std::unordered_map<std::string, FontManager::FaceCacheItem> FontManager::fontCache;
FT_Library FontManager::library = nullptr;

void FontManager::update() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!fonts.empty()) {
        LOG_INFO("FONT_INIT_SKIP", "message", "update skipped (already initialized)");
        return;
    }

    if (library == nullptr) {
        FT_Init_FreeType(&library);
    }

    if (platform == nullptr) {
#ifdef _WIN32
        platform = std::make_unique<Win32FontPlatform>();
#elif defined(__APPLE__)
        platform = std::make_unique<AppleFontPlatform>();
#else
        platform = std::make_unique<LinuxFontPlatform>();
#endif
    }

    const auto startTime = std::chrono::steady_clock::now();
    LOG_INFO("FONT_LOAD_START", "pid", platform->getPid());

    platform->enumerateSystemFonts([&](const std::string& path, uint32_t faceIndex,
                                       const std::string& defaultGlobalName, const std::string& defaultLocalizedName) {
        addFontFile(path, faceIndex, defaultGlobalName, defaultLocalizedName);
    });

    std::ranges::sort(fonts, [](const auto& a, const auto& b) -> bool {
        const bool aHidden = !a.localizedName_.empty() && a.localizedName_[0] == '.';
        const bool bHidden = !b.localizedName_.empty() && b.localizedName_[0] == '.';
        if (aHidden != bHidden) {
            return !aHidden;
        }
        return platform->localeCompare(a.localizedName_, b.localizedName_);
    });

    for (const auto* fallbackPath : platform->getFallbackFonts()) {
        if (std::filesystem::exists(fallbackPath)) {
            (void)getFace(fallbackPath, 0, "");
            break;
        }
    }

    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    LOG_INFO("FONT_LOAD_DONE", "loadedCount", static_cast<int>(fonts.size()), "durationMs", elapsed);
}

auto FontManager::getFontList() -> const std::vector<FontManager::FontInfo>& {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return fonts;
}

auto FontManager::getFontPath(int index) -> std::string {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (index >= 0 && index < static_cast<int>(fonts.size())) {
        return fonts[index].fontPath_;
    }
    return "";
}

auto FontManager::getFace(int choiceIndex) -> FT_Face {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (choiceIndex < 0 || choiceIndex >= static_cast<int>(fonts.size())) {
        return nullptr;
    }

    const auto& fontInformation = fonts[choiceIndex];
    return getFace(fontInformation.fontPath_, fontInformation.faceIndex_, fontInformation.fontName_);
}

auto FontManager::getFaceByNameLocked(const std::string& fontName) -> FT_Face {
    if (!fontName.empty()) {
        auto it = std::ranges::find_if(fonts, [&](const FontInfo& f) -> bool { return f.fontName_ == fontName; });
        if (it != fonts.end()) {
            return getFace(it->fontPath_, it->faceIndex_, fontName);
        }
    }
    return fonts.empty() ? nullptr
                         : getFace(fonts[0].fontPath_, fonts[0].faceIndex_, fonts.empty() ? "" : fonts[0].fontName_);
}

auto FontManager::getFace(const std::string& path, uint32_t faceIndex, const std::string& fontName) -> FT_Face {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (path.empty()) {
        return nullptr;
    }

    std::string cacheKey =
        fontName.empty() ? makeCacheKey(path, faceIndex) : (fontName + "::" + makeCacheKey(path, faceIndex));
    auto cacheIterator = fontCache.find(cacheKey);

    if (cacheIterator != fontCache.end()) {
        lruOrder.splice(lruOrder.begin(), lruOrder, cacheIterator->second.lruIt_);
        return cacheIterator->second.face_;
    }

    FT_Face face = nullptr;
    bool loadSuccess = false;

    if (library == nullptr) {
        FT_Init_FreeType(&library);
    }

    if (library != nullptr) {
        if (FT_New_Face(library, path.c_str(), static_cast<long>(faceIndex), &face) == 0) {
            loadSuccess = true;
        }
    }

    if (!loadSuccess) {
        // Try to load fallbacks (always use faceIndex 0 for fallback fonts)
        for (const auto* fallbackPath : platform->getFallbackFonts()) {
            std::string fallbackCacheKey = makeCacheKey(fallbackPath, 0);
            auto fallbackIterator = fontCache.find(fallbackCacheKey);

            if (fallbackIterator != fontCache.end()) {
                lruOrder.splice(lruOrder.begin(), lruOrder, fallbackIterator->second.lruIt_);
                return fallbackIterator->second.face_;
            }

            if (library != nullptr) {
                if (FT_New_Face(library, fallbackPath, 0, &face) == 0) {
                    lruOrder.push_front(fallbackCacheKey);
                    fontCache.emplace(fallbackCacheKey, FaceCacheItem{face, lruOrder.begin()});
                    loadSuccess = true;
                    break;
                }
            }
        }
    }

    if (loadSuccess && face != nullptr) {
        // --- Apply Variable Font Axis Coordinates ---
        FT_MM_Var* mmVariation = nullptr;
        if (FT_Get_MM_Var(face, &mmVariation) == 0 && mmVariation != nullptr) {
            if (mmVariation->num_namedstyles > 0 && mmVariation->namedstyle != nullptr && !fontName.empty()) {
                FT_Var_Named_Style* matchedStyle = nullptr;
                const FT_UShort systemLanguageId = platform->getSystemLangId();

                // 1. Scan with system language
                for (FT_UInt index = 0; index < mmVariation->num_namedstyles; ++index) {
                    std::string styleName =
                        getSfntNameById(face, mmVariation->namedstyle[index].strid, systemLanguageId);
                    if (!styleName.empty() && fontName.size() >= styleName.size()) {
                        std::string suffix = fontName.substr(fontName.size() - styleName.size());
                        if (suffix == styleName) {
                            matchedStyle = &mmVariation->namedstyle[index];
                            break;
                        }
                    }
                }

                // 2. Fallback to English if not found
                if (matchedStyle == nullptr) {
                    for (FT_UInt index = 0; index < mmVariation->num_namedstyles; ++index) {
                        std::string styleName = getSfntNameById(face, mmVariation->namedstyle[index].strid, 0x0409);
                        if (!styleName.empty() && fontName.size() >= styleName.size()) {
                            std::string suffix = fontName.substr(fontName.size() - styleName.size());
                            if (suffix == styleName) {
                                matchedStyle = &mmVariation->namedstyle[index];
                                break;
                            }
                        }
                    }
                }

                if (matchedStyle != nullptr && matchedStyle->coords != nullptr) {
                    FT_Set_Var_Design_Coordinates(face, mmVariation->num_axis, matchedStyle->coords);
                }
            }
            FT_Done_MM_Var(library, mmVariation);
        }

        if (fontCache.size() >= kFaceCacheCapacity) {
            fontCache.erase(lruOrder.back());  // FT_Done_Face called by FaceCacheItem destructor
            lruOrder.pop_back();
        }
        lruOrder.push_front(cacheKey);
        fontCache.emplace(cacheKey, FaceCacheItem{face, lruOrder.begin()});
        LOG_INFO("FONT_USE", "fontName", fontName, "path", path, "faceIndex", static_cast<int>(faceIndex), "source",
                 "load");
        return face;
    }

    return nullptr;
}

auto FontManager::getFontPathAndIndex(FT_Face face, std::string& outPath, uint32_t& outFaceIndex) -> bool {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (face == nullptr) {
        return false;
    }

    for (const auto& [key, entry] : fontCache) {
        if (entry.face_ == face) {
            std::string realKey = key;
            size_t doubleColon = key.find("::");
            if (doubleColon != std::string::npos) {
                realKey = key.substr(doubleColon + 2);
            }
            size_t sep = realKey.find_last_of('|');
            if (sep != std::string::npos) {
                outPath = realKey.substr(0, sep);
                outFaceIndex = static_cast<uint32_t>(std::stoul(realKey.substr(sep + 1)));
                return true;
            }
        }
    }

    return false;
}

void FontManager::cleanup() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    fontCache.clear();  // FT_Done_Face called by FaceCacheItem destructors
    lruOrder.clear();

    if (library != nullptr) {
        FT_Done_FreeType(library);
        library = nullptr;
    }
    platform.reset();
}

// ---------- private ----------

auto FontManager::makeCacheKey(const std::string& path, uint32_t faceIndex) -> std::string {
    return path + "|" + std::to_string(faceIndex);
}

namespace {

// Decode UTF-16BE bytes from FT_SfntName into a UTF-8 std::string.
auto decodeUtf16BE(const FT_Byte* data, FT_UInt len) -> std::string {
    std::u16string utf16;
    utf16.reserve(len / 2);
    for (FT_UInt i = 0; i + 1 < len; i += 2) {
        uint16_t be;
        std::memcpy(&be, data + i, 2);
        if constexpr (std::endian::native == std::endian::little) {
            be = static_cast<uint16_t>((be >> 8) | (be << 8));
        }
        utf16 += static_cast<char16_t>(be);
    }
    return utf8::utf16to8(utf16);
}

struct SfntNameRecord {
    std::string family16_;     // ID 16: Typographic Family
    std::string subfamily17_;  // ID 17: Typographic Subfamily
    std::string family1_;      // ID 1:  Font Family
    std::string subfamily2_;   // ID 2:  Font Subfamily
};

auto extractSfntString(const FT_SfntName& n) -> std::string {
    if (n.string == nullptr || n.string_len == 0) {
        return {};
    }
    try {
        if (n.platform_id == TT_PLATFORM_MICROSOFT) {
            return decodeUtf16BE(n.string, n.string_len);
        }
        std::string rawStr(reinterpret_cast<const char*>(n.string), n.string_len);
        if (utf8::is_valid(rawStr.begin(), rawStr.end())) {
            return rawStr;
        }
    } catch (...) {
        // Safe fallback for exceptions thrown during conversion
    }
    return {};
}

auto getMacLanguageId(FT_UShort windowsLcid) -> FT_UShort {
    if (windowsLcid == 0x0411) {
        return 11;  // Japanese
    }
    if (windowsLcid == 0x0404) {
        return 2;  // Traditional Chinese
    }
    if (windowsLcid == 0x0804) {
        return 33;  // Simplified Chinese
    }
    if (windowsLcid == 0x0412) {
        return 23;  // Korean
    }
    if (windowsLcid == 0x040C) {
        return 1;  // French
    }
    if (windowsLcid == 0x0407) {
        return 2;  // German
    }
    return 0;  // Default/English
}

auto collectSfntNames(FT_Face face, FT_UShort langId) -> SfntNameRecord {
    SfntNameRecord rec;
    FT_UInt count = FT_Get_Sfnt_Name_Count(face);
    for (FT_UInt i = 0; i < count; ++i) {
        FT_SfntName n{};
        if (FT_Get_Sfnt_Name(face, i, &n) != 0) {
            continue;
        }

        // If langId is 0xFFFF, skip platform and language checks (Priority 3 fallback)
        if (langId != 0xFFFF) {
            if (n.platform_id != TT_PLATFORM_MICROSOFT && n.platform_id != TT_PLATFORM_MACINTOSH) {
                continue;
            }
            bool isMatch = false;
            if (n.platform_id == TT_PLATFORM_MICROSOFT && n.language_id == langId) {
                isMatch = true;
            } else if (n.platform_id == TT_PLATFORM_MACINTOSH && n.language_id == getMacLanguageId(langId)) {
                isMatch = true;
            }
            if (!isMatch) {
                continue;
            }
        }

        if (n.name_id == TT_NAME_ID_TYPOGRAPHIC_FAMILY && rec.family16_.empty()) {
            rec.family16_ = extractSfntString(n);
        } else if (n.name_id == TT_NAME_ID_TYPOGRAPHIC_SUBFAMILY && rec.subfamily17_.empty()) {
            rec.subfamily17_ = extractSfntString(n);
        } else if (n.name_id == TT_NAME_ID_FONT_FAMILY && rec.family1_.empty()) {
            rec.family1_ = extractSfntString(n);
        } else if (n.name_id == TT_NAME_ID_FONT_SUBFAMILY && rec.subfamily2_.empty()) {
            rec.subfamily2_ = extractSfntString(n);
        }
    }
    return rec;
}

auto buildFamilyName(const SfntNameRecord& rec) -> std::string {
    // Prefer ID 16+17 (typographic), fall back to ID 1+2 (legacy)
    auto compose = [](const std::string& base, const std::string& sub) -> std::string {
        if (!sub.empty() && sub != "Regular") {
            std::string name = base;
            name += ' ';
            name += sub;
            return name;
        }
        return base;
    };
    if (!rec.family16_.empty()) {
        return compose(rec.family16_, rec.subfamily17_);
    }
    if (!rec.family1_.empty()) {
        return compose(rec.family1_, rec.subfamily2_);
    }
    return {};
}

auto buildBaseFamilyName(const SfntNameRecord& rec) -> std::string {
    if (!rec.family16_.empty()) {
        return rec.family16_;
    }
    if (!rec.family1_.empty()) {
        return rec.family1_;
    }
    return {};
}

auto getSfntBaseFamilyName(FT_Face face, FT_UShort langId) -> std::string {
    constexpr FT_UShort kEnglish = 0x0409;
    for (FT_UShort targetLang : {langId, kEnglish}) {
        std::string name = buildBaseFamilyName(collectSfntNames(face, targetLang));
        if (!name.empty()) {
            return name;
        }
        if (targetLang == kEnglish) {
            break;
        }
    }
    std::string fallbackName = buildBaseFamilyName(collectSfntNames(face, 0xFFFF));
    if (!fallbackName.empty()) {
        return fallbackName;
    }
    return {};
}

// Extract a family+subfamily display string from FT_SfntName records for the given language.
// Prefers name ID 16+17 (typographic), falls back to name ID 1+2. Returns empty if not found.
auto getSfntFamilyName(FT_Face face, FT_UShort langId) -> std::string {
    constexpr FT_UShort kEnglish = 0x0409;
    for (FT_UShort targetLang : {langId, kEnglish}) {
        std::string name = buildFamilyName(collectSfntNames(face, targetLang));
        if (!name.empty()) {
            return name;
        }
        if (targetLang == kEnglish) {
            break;
        }
    }
    std::string fallbackName = buildFamilyName(collectSfntNames(face, 0xFFFF));
    if (!fallbackName.empty()) {
        return fallbackName;
    }
    return {};
}

auto getSfntNameById(FT_Face face, FT_UShort nameIdentifier, FT_UShort languageId) -> std::string {
    auto getNameInternal = [&](FT_UShort targetLang) -> std::string {
        FT_UInt nameCount = FT_Get_Sfnt_Name_Count(face);
        for (FT_UInt index = 0; index < nameCount; ++index) {
            FT_SfntName sfntName{};
            if (FT_Get_Sfnt_Name(face, index, &sfntName) != 0) {
                continue;
            }

            if (targetLang != 0xFFFF) {
                bool isMatch = false;
                if (sfntName.platform_id == TT_PLATFORM_MICROSOFT && sfntName.language_id == targetLang) {
                    isMatch = true;
                } else if (sfntName.platform_id == TT_PLATFORM_MACINTOSH &&
                           sfntName.language_id == getMacLanguageId(targetLang)) {
                    isMatch = true;
                }
                if (!isMatch) {
                    continue;
                }
            }

            if (sfntName.name_id == nameIdentifier) {
                return extractSfntString(sfntName);
            }
        }
        return {};
    };

    std::string resultName = getNameInternal(languageId);
    if (resultName.empty() && languageId != 0x0409) {
        resultName = getNameInternal(0x0409);
    }
    if (resultName.empty()) {
        resultName = getNameInternal(0xFFFF);
    }
    return resultName;
}

}  // namespace

auto FontManager::loadFontNames(const std::string& path, uint32_t faceIndex, const std::string& fallback) -> FontNames {
    if (library == nullptr) {
        FT_Init_FreeType(&library);
    }
    if (library == nullptr) {
        return {fallback, fallback};
    }

    FT_Face face = nullptr;
    if (FT_New_Face(library, path.c_str(), static_cast<long>(faceIndex), &face) != 0) {
        return {fallback, fallback};
    }

    const FT_UShort systemLang = platform->getSystemLangId();
    std::string globalName = getSfntFamilyName(face, 0x0409);         // English
    std::string localizedName = getSfntFamilyName(face, systemLang);  // OS locale

    if (globalName.empty() || !utf8::is_valid(globalName.begin(), globalName.end())) {
        globalName = fallback;
    }
    if (localizedName.empty() || !utf8::is_valid(localizedName.begin(), localizedName.end())) {
        localizedName = globalName;
    }

    FT_Done_Face(face);
    return {globalName, localizedName};
}

void FontManager::addFontFile(const std::string& path, uint32_t faceIndex, const std::string& defaultGlobalName,
                              const std::string& defaultLocalizedName) {
    const auto isDuplicateFile = std::ranges::any_of(fonts, [&](const FontInfo& fontItem) -> bool {
        return fontItem.fontPath_ == path && fontItem.faceIndex_ == faceIndex;
    });
    if (isDuplicateFile) {
        return;
    }

    bool isVariable = false;
    if (path.size() >= 4) {
        std::string extension = path.substr(path.size() - 4);
        std::ranges::transform(extension, extension.begin(),
                               [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (extension == ".ttf" || extension == ".otf") {
            isVariable = true;
        }
    }

    bool scannedVariable = false;

    if (isVariable) {
        if (library == nullptr) {
            FT_Init_FreeType(&library);
        }
        if (library != nullptr) {
            FT_Face face = nullptr;
            if (FT_New_Face(library, path.c_str(), static_cast<long>(faceIndex), &face) == 0) {
                FT_MM_Var* mmVariation = nullptr;
                if (FT_Get_MM_Var(face, &mmVariation) == 0 && mmVariation != nullptr) {
                    if (mmVariation->num_namedstyles > 0 && mmVariation->namedstyle != nullptr) {
                        scannedVariable = true;
                        const FT_UShort systemLanguageId = platform->getSystemLangId();

                        for (FT_UInt styleIndex = 0; styleIndex < mmVariation->num_namedstyles; ++styleIndex) {
                            const auto& style = mmVariation->namedstyle[styleIndex];
                            std::string styleName = getSfntNameById(face, style.strid, systemLanguageId);
                            std::string styleNameEnglish = getSfntNameById(face, style.strid, 0x0409);
                            std::string familyName = getSfntBaseFamilyName(face, systemLanguageId);
                            std::string familyNameEnglish = getSfntBaseFamilyName(face, 0x0409);

                            if (familyName.empty()) {
                                familyName = styleName;
                            }
                            if (familyNameEnglish.empty()) {
                                familyNameEnglish = styleNameEnglish;
                            }

                            auto trimWhitespace = [](const std::string& sourceString) -> std::string {
                                const auto firstIndex = sourceString.find_first_not_of(" \t\r\n");
                                if (firstIndex == std::string::npos) {
                                    return {};
                                }
                                const auto lastIndex = sourceString.find_last_not_of(" \t\r\n");
                                return sourceString.substr(firstIndex, (lastIndex - firstIndex + 1));
                            };

                            std::string globalStyleName = trimWhitespace(familyNameEnglish + " " + styleNameEnglish);
                            std::string localizedStyleName = trimWhitespace(familyName + " " + styleName);

                            if (globalStyleName.empty() ||
                                !utf8::is_valid(globalStyleName.begin(), globalStyleName.end())) {
                                continue;
                            }
                            if (localizedStyleName.empty() ||
                                !utf8::is_valid(localizedStyleName.begin(), localizedStyleName.end())) {
                                localizedStyleName = globalStyleName;
                            }

                            // Sanitize null characters to prevent crash in OFX/Host C-style string parsing
                            auto sanitizeString = [](std::string stringValue) -> std::string {
                                const size_t nullPosition = stringValue.find('\0');
                                if (nullPosition != std::string::npos) {
                                    stringValue = stringValue.substr(0, nullPosition);
                                }
                                return stringValue;
                            };
                            globalStyleName = sanitizeString(globalStyleName);
                            localizedStyleName = sanitizeString(localizedStyleName);

                            const auto isDuplicate = std::ranges::any_of(fonts, [&](const FontInfo& fontItem) -> bool {
                                return fontItem.fontName_ == globalStyleName;
                            });
                            if (!isDuplicate) {
                                LOG_INFO("FONT_ENUM", "fontName", globalStyleName, "path", path, "faceIndex",
                                         static_cast<int>(faceIndex));
                                fonts.push_back({.fontName_ = globalStyleName,
                                                 .localizedName_ = localizedStyleName,
                                                 .fontPath_ = path,
                                                 .faceIndex_ = faceIndex});
                            }
                        }
                    }
                    FT_Done_MM_Var(library, mmVariation);
                }
                FT_Done_Face(face);
            }
        }
    }

    if (!scannedVariable) {
        auto sanitizeString = [](std::string stringValue) -> std::string {
            const size_t nullPosition = stringValue.find('\0');
            if (nullPosition != std::string::npos) {
                stringValue = stringValue.substr(0, nullPosition);
            }
            return stringValue;
        };
        std::string finalGlobalName = sanitizeString(defaultGlobalName);
        std::string finalLocalizedName = sanitizeString(defaultLocalizedName);

        if (finalGlobalName.empty() || !utf8::is_valid(finalGlobalName.begin(), finalGlobalName.end())) {
            finalGlobalName = std::filesystem::path(path).filename().string();
        }
        if (finalLocalizedName.empty() || !utf8::is_valid(finalLocalizedName.begin(), finalLocalizedName.end())) {
            finalLocalizedName = finalGlobalName;
        }

        const auto isDuplicate = std::ranges::any_of(
            fonts, [&](const FontInfo& fontItem) -> bool { return fontItem.fontName_ == finalGlobalName; });
        if (!isDuplicate) {
            LOG_INFO("FONT_ENUM", "fontName", finalGlobalName, "path", path, "faceIndex", static_cast<int>(faceIndex));
            fonts.push_back({.fontName_ = finalGlobalName,
                             .localizedName_ = finalLocalizedName,
                             .fontPath_ = path,
                             .faceIndex_ = faceIndex});
        }
    }
}

}  // namespace MugLab::OfxBase
