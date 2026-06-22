#include "LinuxFontPlatform.h"

#include <fontconfig/fontconfig.h>
#include <langinfo.h>
#include <locale.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>

namespace MugLab::OfxBase {

namespace {

const std::vector<const char*> kLinuxFallbackFonts = {
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
};

}  // namespace

LinuxFontPlatform::LinuxFontPlatform() {
}

LinuxFontPlatform::~LinuxFontPlatform() {
}

auto LinuxFontPlatform::getFallbackFonts() const -> const std::vector<const char*>& {
    return kLinuxFallbackFonts;
}

auto LinuxFontPlatform::localeCompare(const std::string& firstString, const std::string& secondString) const -> bool {
    return strcoll(firstString.c_str(), secondString.c_str()) < 0;
}

auto LinuxFontPlatform::getSystemLangId() const -> uint16_t {
    const char* localeName = setlocale(LC_MESSAGES, nullptr);
    if (localeName == nullptr) {
        return 0x0409;
    }
    std::string localeString(localeName);
    uint16_t languageId = 0x0409;
    if (localeString.starts_with("ja")) {
        languageId = 0x0411;
    } else if (localeString.starts_with("zh_CN")) {
        languageId = 0x0804;
    } else if (localeString.starts_with("zh_TW")) {
        languageId = 0x0404;
    } else if (localeString.starts_with("ko")) {
        languageId = 0x0412;
    } else if (localeString.starts_with("fr")) {
        languageId = 0x040C;
    } else if (localeString.starts_with("de")) {
        languageId = 0x0407;
    } else if (localeString.starts_with("es")) {
        languageId = 0x0C0A;
    }
    return languageId;
}

auto LinuxFontPlatform::getPid() const -> int {
    return static_cast<int>(getpid());
}

void LinuxFontPlatform::enumerateSystemFonts(
    const std::function<void(const std::string& path, uint32_t faceIndex, const std::string& defaultGlobalName,
                             const std::string& defaultLocalizedName)>& addFontCallback) {
    FcConfig* fontconfigConfig = FcInitLoadConfigAndFonts();
    if (fontconfigConfig == nullptr) {
        return;
    }

    FcPattern* fontconfigPattern = FcPatternCreate();
    FcObjectSet* fontconfigObjectSet = FcObjectSetBuild(FC_FILE, FC_INDEX, nullptr);
    FcFontSet* fontconfigFontSet = FcFontList(fontconfigConfig, fontconfigPattern, fontconfigObjectSet);

    if (fontconfigFontSet != nullptr) {
        for (int fontIndex = 0; fontIndex < fontconfigFontSet->nfont; ++fontIndex) {
            FcPattern* fontPattern = fontconfigFontSet->fonts[fontIndex];

            FcChar8* filePath = nullptr;
            if (FcPatternGetString(fontPattern, FC_FILE, 0, &filePath) != FcResultMatch || filePath == nullptr) {
                continue;
            }

            int faceIndex = 0;
            FcPatternGetInteger(fontPattern, FC_INDEX, 0, &faceIndex);

            addFontCallback(reinterpret_cast<const char*>(filePath), static_cast<uint32_t>(faceIndex), "", "");
        }
        FcFontSetDestroy(fontconfigFontSet);
    }

    FcObjectSetDestroy(fontconfigObjectSet);
    FcPatternDestroy(fontconfigPattern);
    FcConfigDestroy(fontconfigConfig);
}

}  // namespace MugLab::OfxBase
