#pragma once

#include "FontPlatform.h"

namespace MugLab::OfxBase {

class Win32FontPlatform final : public FontPlatform {
   public:
    Win32FontPlatform();
    ~Win32FontPlatform() override;

    [[nodiscard]] auto getFallbackFonts() const -> const std::vector<const char*>& override;
    [[nodiscard]] auto localeCompare(const std::string& firstString,
                                     const std::string& secondString) const -> bool override;
    [[nodiscard]] auto getSystemLangId() const -> uint16_t override;
    [[nodiscard]] auto getPid() const -> int override;
    void enumerateSystemFonts(
        const std::function<void(const std::string& path, uint32_t faceIndex, const std::string& defaultGlobalName,
                                 const std::string& defaultLocalizedName)>& addFontCallback) override;
};

}  // namespace MugLab::OfxBase
