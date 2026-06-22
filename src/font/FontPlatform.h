#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MugLab::OfxBase {

class FontPlatform {
   public:
    virtual ~FontPlatform();

    [[nodiscard]] virtual auto getFallbackFonts() const -> const std::vector<const char*>& = 0;
    [[nodiscard]] virtual auto localeCompare(const std::string& firstString,
                                             const std::string& secondString) const -> bool = 0;
    [[nodiscard]] virtual auto getSystemLangId() const -> uint16_t = 0;
    [[nodiscard]] virtual auto getPid() const -> int = 0;
    virtual void enumerateSystemFonts(
        const std::function<void(const std::string& path, uint32_t faceIndex, const std::string& defaultGlobalName,
                                 const std::string& defaultLocalizedName)>& addFontCallback) = 0;
};

}  // namespace MugLab::OfxBase
