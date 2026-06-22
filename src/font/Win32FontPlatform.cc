#include "Win32FontPlatform.h"

#include <dwrite.h>
#include <processthreadsapi.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <filesystem>

namespace MugLab::OfxBase {

namespace {

const std::vector<const char*> kWin32FallbackFonts = {
    "C:/Windows/Fonts/msgothic.ttc",
    "C:/Windows/Fonts/msmincho.ttc",
};

auto wideToUtf8(const wchar_t* wideString) -> std::string {
    int utf8StringSize = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, nullptr, 0, nullptr, nullptr);
    if (utf8StringSize <= 0) {
        return {};
    }
    std::string utf8String(utf8StringSize - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideString, -1, utf8String.data(), utf8StringSize, nullptr, nullptr);
    return utf8String;
}

auto composeFamilyName(const std::string& familyName, const std::string& subfamilyName) -> std::string {
    std::string composedName = familyName;
    if (!subfamilyName.empty() && subfamilyName != "Regular") {
        composedName += ' ';
        composedName += subfamilyName;
    }
    return composedName;
}

auto getLocalizedString(IDWriteLocalizedStrings* localizedStrings, const std::wstring& localeName) -> std::string {
    UINT32 stringIndex = 0;
    WINBOOL doesLocaleExist = FALSE;
    if (FAILED(localizedStrings->FindLocaleName(localeName.c_str(), &stringIndex, &doesLocaleExist)) ||
        doesLocaleExist == FALSE) {
        if (FAILED(localizedStrings->FindLocaleName(L"en-us", &stringIndex, &doesLocaleExist)) ||
            doesLocaleExist == FALSE) {
            stringIndex = 0;
        }
    }
    std::string resultString;
    UINT32 stringLength = 0;
    if (SUCCEEDED(localizedStrings->GetStringLength(stringIndex, &stringLength))) {
        std::wstring wideString(stringLength + 1, L'\0');
        if (SUCCEEDED(localizedStrings->GetString(stringIndex, wideString.data(), stringLength + 1))) {
            resultString = wideToUtf8(wideString.c_str());
        }
    }
    return resultString;
}

}  // namespace

Win32FontPlatform::Win32FontPlatform() = default;

Win32FontPlatform::~Win32FontPlatform() = default;

auto Win32FontPlatform::getFallbackFonts() const -> const std::vector<const char*>& {
    return kWin32FallbackFonts;
}

auto Win32FontPlatform::localeCompare(const std::string& firstString, const std::string& secondString) const -> bool {
    auto toWide = [](const std::string& sourceString) -> std::wstring {
        int wideStringLength = MultiByteToWideChar(CP_UTF8, 0, sourceString.c_str(), -1, nullptr, 0);
        std::wstring wideString(wideStringLength, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, sourceString.c_str(), -1, wideString.data(), wideStringLength);
        return wideString;
    };
    int compareResult = CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE, toWide(firstString).c_str(), -1,
                                        toWide(secondString).c_str(), -1, nullptr, nullptr, 0);
    return compareResult == CSTR_LESS_THAN;
}

auto Win32FontPlatform::getSystemLangId() const -> uint16_t {
    return static_cast<uint16_t>(GetUserDefaultUILanguage());
}

auto Win32FontPlatform::getPid() const -> int {
    return static_cast<int>(GetCurrentProcessId());
}

void Win32FontPlatform::enumerateSystemFonts(
    const std::function<void(const std::string& path, uint32_t faceIndex, const std::string& defaultGlobalName,
                             const std::string& defaultLocalizedName)>& addFontCallback) {
    using Microsoft::WRL::ComPtr;

    ComPtr<IDWriteFactory> dwriteFactory;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf())))) {
        return;
    }

    ComPtr<IDWriteFontCollection> fontCollection;
    if (FAILED(dwriteFactory->GetSystemFontCollection(fontCollection.GetAddressOf()))) {
        return;
    }

    wchar_t localeNameBuffer[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(localeNameBuffer, LOCALE_NAME_MAX_LENGTH) == 0) {
        wcscpy_s(localeNameBuffer, L"en-us");
    }
    const std::wstring userLocaleName(localeNameBuffer);

    ComPtr<IDWriteLocalFontFileLoader> localFontFileLoader;

    const UINT32 familyCount = fontCollection->GetFontFamilyCount();
    for (UINT32 familyIndex = 0; familyIndex < familyCount; ++familyIndex) {
        ComPtr<IDWriteFontFamily> fontFamily;
        if (FAILED(fontCollection->GetFontFamily(familyIndex, fontFamily.GetAddressOf()))) {
            continue;
        }

        const UINT32 fontCount = fontFamily->GetFontCount();
        for (UINT32 fontIndex = 0; fontIndex < fontCount; ++fontIndex) {
            ComPtr<IDWriteFont> font;
            if (FAILED(fontFamily->GetFont(fontIndex, font.GetAddressOf()))) {
                continue;
            }

            ComPtr<IDWriteFontFace> fontFace;
            if (FAILED(font->CreateFontFace(fontFace.GetAddressOf()))) {
                continue;
            }

            UINT32 fileCount = 0;
            fontFace->GetFiles(&fileCount, nullptr);
            if (fileCount == 0) {
                continue;
            }

            std::vector<ComPtr<IDWriteFontFile>> fontFiles(fileCount);
            std::vector<IDWriteFontFile*> rawFontFiles(fileCount);
            fontFace->GetFiles(&fileCount, rawFontFiles.data());
            for (UINT32 fileIndex = 0; fileIndex < fileCount; ++fileIndex) {
                fontFiles[fileIndex].Attach(rawFontFiles[fileIndex]);
            }

            const void* referenceKey = nullptr;
            UINT32 referenceKeySize = 0;
            if (FAILED(fontFiles[0]->GetReferenceKey(&referenceKey, &referenceKeySize))) {
                continue;
            }

            ComPtr<IDWriteFontFileLoader> fontFileLoader;
            if (FAILED(fontFiles[0]->GetLoader(fontFileLoader.GetAddressOf()))) {
                continue;
            }
            if (FAILED(fontFileLoader.As(&localFontFileLoader))) {
                continue;
            }

            UINT32 filePathLength = 0;
            localFontFileLoader->GetFilePathLengthFromKey(referenceKey, referenceKeySize, &filePathLength);
            std::wstring wideFilePath(filePathLength + 1, L'\0');
            if (FAILED(localFontFileLoader->GetFilePathFromKey(referenceKey, referenceKeySize, wideFilePath.data(),
                                                               filePathLength + 1))) {
                continue;
            }
            std::ranges::replace(wideFilePath, L'\\', L'/');
            std::string filePath = wideToUtf8(wideFilePath.c_str());
            if (filePath.empty()) {
                continue;
            }

            uint32_t faceIndex = fontFace->GetIndex();

            auto getDWriteName = [&](DWRITE_INFORMATIONAL_STRING_ID familyInformationalId,
                                     DWRITE_INFORMATIONAL_STRING_ID subfamilyInformationalId,
                                     const std::wstring& localeName) -> std::string {
                ComPtr<IDWriteLocalizedStrings> familyStrings;
                WINBOOL hasFamily = FALSE;
                if (FAILED(font->GetInformationalStrings(familyInformationalId, familyStrings.GetAddressOf(),
                                                         &hasFamily)) ||
                    hasFamily == FALSE) {
                    return {};
                }
                std::string familyName = getLocalizedString(familyStrings.Get(), localeName);
                if (familyName.empty()) {
                    return {};
                }

                ComPtr<IDWriteLocalizedStrings> subfamilyStrings;
                WINBOOL hasSubfamily = FALSE;
                std::string subfamilyName;
                if (SUCCEEDED(font->GetInformationalStrings(subfamilyInformationalId, subfamilyStrings.GetAddressOf(),
                                                            &hasSubfamily)) &&
                    hasSubfamily != FALSE) {
                    subfamilyName = getLocalizedString(subfamilyStrings.Get(), localeName);
                }
                return composeFamilyName(familyName, subfamilyName);
            };

            std::string globalName = getDWriteName(DWRITE_INFORMATIONAL_STRING_PREFERRED_FAMILY_NAMES,
                                                   DWRITE_INFORMATIONAL_STRING_PREFERRED_SUBFAMILY_NAMES, L"en-us");
            if (globalName.empty()) {
                globalName = getDWriteName(DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES,
                                           DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES, L"en-us");
            }
            if (globalName.empty()) {
                globalName = std::filesystem::path(filePath).filename().string();
            }

            std::string localizedName =
                getDWriteName(DWRITE_INFORMATIONAL_STRING_PREFERRED_FAMILY_NAMES,
                              DWRITE_INFORMATIONAL_STRING_PREFERRED_SUBFAMILY_NAMES, userLocaleName);
            if (localizedName.empty()) {
                localizedName = getDWriteName(DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES,
                                              DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES, userLocaleName);
            }
            if (localizedName.empty()) {
                localizedName = globalName;
            }

            addFontCallback(filePath, faceIndex, globalName, localizedName);
        }
    }
}

}  // namespace MugLab::OfxBase
