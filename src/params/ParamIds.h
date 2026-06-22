#pragma once

// Stable string identifiers for all OFX parameters.
// These IDs are persisted in DaVinci Resolve project files —
// changing them will break existing projects.

namespace MugLab::OfxBase::ParamIds {

// ---------------------------------------------------------------------------
// Page selector (pseudo-page system)
// ---------------------------------------------------------------------------
inline constexpr const char* kPageSelect = "pageSelect";

inline constexpr const char* kPageText   = "Text";
inline constexpr const char* kPageLayout = "Layout";
inline constexpr const char* kPageStyle  = "Style";

// ---------------------------------------------------------------------------
// Text page
// ---------------------------------------------------------------------------
inline constexpr const char* kTextGroup   = "textGroup";
inline constexpr const char* kTextContent = "textContent";
inline constexpr const char* kFontName    = "fontName";
inline constexpr const char* kFontSize    = "fontSize";

// ---------------------------------------------------------------------------
// Layout page
// ---------------------------------------------------------------------------
inline constexpr const char* kLayoutGroup = "layoutGroup";
inline constexpr const char* kPosition    = "position";
inline constexpr const char* kSize        = "size";

// ---------------------------------------------------------------------------
// Style page
// Demonstrates multi-condition secret switching:
//   kColor is visible only when BOTH kPageSelect == "Style"
//   AND kUseCustomColor == true (AND logic).
// ---------------------------------------------------------------------------
inline constexpr const char* kStyleGroup      = "styleGroup";
inline constexpr const char* kUseCustomColor  = "useCustomColor";
inline constexpr const char* kColor           = "color";
// kOpacity is visible whenever kPageSelect == "Style",
// regardless of kUseCustomColor (single-condition, always shown on style page).
inline constexpr const char* kOpacity         = "opacity";

// ---------------------------------------------------------------------------
// Shadow slots (Style page)
// Demonstrates the slot system: N identical GroupDecls share the same slot
// roles (ShadowSlot enum) but carry unique string IDs via index suffix.
// Access pattern:
//   auto& grp = root_.findParameter<GroupParameter>("shadowGroup0");
//   grp.getParameter<BooleanParameter>(ShadowSlot::kEnabled)->get(time);
// ---------------------------------------------------------------------------
inline constexpr const char* kShadowGroupPrefix   = "shadowGroup";    // + "0", "1", ...
inline constexpr const char* kShadowEnabledPrefix = "shadowEnabled";  // + "0", "1", ...
inline constexpr const char* kShadowOffsetPrefix  = "shadowOffset";   // + "0", "1", ...
inline constexpr const char* kShadowColorPrefix   = "shadowColor";    // + "0", "1", ...

inline constexpr int kShadowSlotCount = 2;

}  // namespace MugLab::OfxBase::ParamIds

// Slot indices within each shadow GroupParameter.
// Use these with GroupParameter::getParameter<T>(slot).
namespace MugLab::OfxBase {
enum class ShadowSlot : int {
    kEnabled = 0,
    kOffset  = 1,
    kColor   = 2,
};
}  // namespace MugLab::OfxBase
