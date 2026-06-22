#include "AppleFontPlatform.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>

namespace MugLab::OfxBase {

namespace {

const std::vector<const char*> kAppleFallbackFonts = {
    "/System/Library/Fonts/PingFang.ttc",
    "/Library/Fonts/Arial Unicode.ttf",
};

}  // namespace

AppleFontPlatform::AppleFontPlatform() {
}

AppleFontPlatform::~AppleFontPlatform() {
}

auto AppleFontPlatform::getFallbackFonts() const -> const std::vector<const char*>& {
    return kAppleFallbackFonts;
}

auto AppleFontPlatform::localeCompare(const std::string& firstString, const std::string& secondString) const -> bool {
    CFStringRef firstCFString = CFStringCreateWithCString(nullptr, firstString.c_str(), kCFStringEncodingUTF8);
    CFStringRef secondCFString = CFStringCreateWithCString(nullptr, secondString.c_str(), kCFStringEncodingUTF8);
    if (firstCFString == nullptr || secondCFString == nullptr) {
        if (firstCFString != nullptr) {
            CFRelease(firstCFString);
        }
        if (secondCFString != nullptr) {
            CFRelease(secondCFString);
        }
        return firstString < secondString;
    }
    CFComparisonResult comparisonResult =
        CFStringCompare(firstCFString, secondCFString, kCFCompareCaseInsensitive | kCFCompareLocalized);
    CFRelease(firstCFString);
    CFRelease(secondCFString);
    return comparisonResult == kCFCompareLessThan;
}

auto AppleFontPlatform::getSystemLangId() const -> uint16_t {
    uint16_t languageId = 0x0409;
    CFArrayRef preferredLanguages = CFLocaleCopyPreferredLanguages();
    if (preferredLanguages != nullptr) {
        if (CFArrayGetCount(preferredLanguages) > 0) {
            auto* languageCFString = static_cast<CFStringRef>(CFArrayGetValueAtIndex(preferredLanguages, 0));
            char languageBuffer[16] = {};
            if (CFStringGetCString(languageCFString, languageBuffer, sizeof(languageBuffer), kCFStringEncodingUTF8)) {
                std::string languageCode(languageBuffer);
                if (languageCode.starts_with("ja")) {
                    languageId = 0x0411;
                } else if (languageCode.starts_with("zh-Hans")) {
                    languageId = 0x0804;
                } else if (languageCode.starts_with("zh-Hant")) {
                    languageId = 0x0404;
                } else if (languageCode.starts_with("ko")) {
                    languageId = 0x0412;
                } else if (languageCode.starts_with("fr")) {
                    languageId = 0x040C;
                } else if (languageCode.starts_with("de")) {
                    languageId = 0x0407;
                } else if (languageCode.starts_with("es")) {
                    languageId = 0x0C0A;
                }
            }
        }
        CFRelease(preferredLanguages);
    }
    return languageId;
}

auto AppleFontPlatform::getPid() const -> int {
    return static_cast<int>(getpid());
}

void AppleFontPlatform::enumerateSystemFonts(
    const std::function<void(const std::string& path, uint32_t faceIndex, const std::string& defaultGlobalName,
                             const std::string& defaultLocalizedName)>& addFontCallback) {
    CTFontCollectionRef fontCollection = CTFontCollectionCreateFromAvailableFonts(nullptr);
    if (fontCollection == nullptr) {
        return;
    }

    CFArrayRef fontDescriptors = CTFontCollectionCreateMatchingFontDescriptors(fontCollection);
    if (fontDescriptors == nullptr) {
        CFRelease(fontCollection);
        return;
    }

    CFIndex descriptorCount = CFArrayGetCount(fontDescriptors);
    for (CFIndex index = 0; index < descriptorCount; ++index) {
        auto* fontDescriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fontDescriptors, index));

        CFURLRef fontUrl = static_cast<CFURLRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontURLAttribute));
        if (fontUrl == nullptr) {
            continue;
        }

        char pathBuffer[4096];
        if (!CFURLGetFileSystemRepresentation(fontUrl, true, reinterpret_cast<UInt8*>(pathBuffer),
                                              sizeof(pathBuffer))) {
            CFRelease(fontUrl);
            continue;
        }
        std::string path(pathBuffer);
        CFRelease(fontUrl);

        CFStringRef displayNameCFString = static_cast<CFStringRef>(
            CTFontDescriptorCopyLocalizedAttribute(fontDescriptor, kCTFontDisplayNameAttribute, nullptr));
        std::string localizedName;
        if (displayNameCFString != nullptr) {
            char nameBuffer[256];
            if (CFStringGetCString(displayNameCFString, nameBuffer, sizeof(nameBuffer), kCFStringEncodingUTF8)) {
                localizedName = nameBuffer;
            }
            CFRelease(displayNameCFString);
        }

        int faceIndex = 0;
        CFNumberRef ttcIndexCFNumber =
            static_cast<CFNumberRef>(CTFontDescriptorCopyAttribute(fontDescriptor, CFSTR("NSFontTTCIndexAttribute")));
        if (ttcIndexCFNumber != nullptr) {
            CFNumberGetValue(ttcIndexCFNumber, kCFNumberIntType, &faceIndex);
            CFRelease(ttcIndexCFNumber);
        }

        std::string globalName;
        CFStringRef familyNameCFString =
            static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontFamilyNameAttribute));
        CFStringRef subfamilyNameCFString =
            static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontStyleNameAttribute));
        if (familyNameCFString != nullptr) {
            char nameBuffer[256];
            if (CFStringGetCString(familyNameCFString, nameBuffer, sizeof(nameBuffer), kCFStringEncodingUTF8)) {
                globalName = nameBuffer;
            }
            CFRelease(familyNameCFString);
        }
        if (subfamilyNameCFString != nullptr) {
            char nameBuffer[256];
            if (CFStringGetCString(subfamilyNameCFString, nameBuffer, sizeof(nameBuffer), kCFStringEncodingUTF8)) {
                std::string subfamilyName(nameBuffer);
                if (!globalName.empty() && !subfamilyName.empty() && subfamilyName != "Regular") {
                    globalName += ' ';
                    globalName += subfamilyName;
                }
            }
            CFRelease(subfamilyNameCFString);
        }
        if (globalName.empty()) {
            globalName = std::filesystem::path(path).filename().string();
        }
        if (localizedName.empty()) {
            localizedName = globalName;
        }

        addFontCallback(path, static_cast<uint32_t>(faceIndex), globalName, localizedName);
    }

    CFRelease(fontDescriptors);
    CFRelease(fontCollection);
}

}  // namespace MugLab::OfxBase
