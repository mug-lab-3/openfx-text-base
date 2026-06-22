#include "ParameterManager.h"

#include "ParamIds.h"
#include "core/Double2DParameter.h"
#include "core/DoubleParameter.h"
#include "core/ParameterDecl.h"
#include "core/ParameterTreeBuilder.h"
#include "font/FontManager.h"

namespace MugLab::OfxBase {

// ---------------------------------------------------------------------------
// Declarative parameter tree
//
// Structure:
//   [pageSelect]   ChoiceParam  — pseudo-page selector (top-level, always visible)
//   [textGroup]    GroupDecl    — visible when pageSelect == "Text"
//     [textContent]  StringParam
//     [fontName]     ChoiceParam  (font list from FontManager)
//     [fontSize]     DoubleParam
//   [layoutGroup]  GroupDecl    — visible when pageSelect == "Layout"
//     [position]     Double2DParam
//     [size]         DoubleParam
//   [styleGroup]   GroupDecl    — visible when pageSelect == "Style"
//     [opacity]      DoubleParam  — visible when pageSelect == "Style"
//     [useCustomColor] BooleanParam
//     [color]        RGBAParam   — visible when BOTH pageSelect=="Style"
//                                  AND useCustomColor==true  (AND logic demo)
//
// NOTE — Double2DParam (and DoubleParam) requires range_, displayRange_, and
// increment_ to be set explicitly. Without these, Resolve's inspector shows no
// slider and only accepts direct text input. range_ sets the hard clamp,
// displayRange_ sets the slider travel, and increment_ sets the drag step.
// ---------------------------------------------------------------------------
static auto makeDecl() -> RootDecl {
    // Font options are loaded from FontManager at describe-time.
    // If the font list is empty a placeholder entry is shown.
    auto fontOptions = std::vector<std::string>{};
    for (const auto& info : FontManager::getFontList()) {
        fontOptions.push_back(info.fontName_);
    }
    if (fontOptions.empty()) {
        fontOptions.emplace_back("(no fonts found)");
    }

    // Visibility rule helpers
    // Builds one shadow GroupDecl for the given slot index.
    // Each slot gets unique string IDs (prefix + index) but shared ShadowSlot roles,
    // so callers can use GroupParameter::getParameter<T>(ShadowSlot::kOffset) etc.
    auto makeShadowGroup = [](int index) -> GroupDecl {
        const std::string s = std::to_string(index);
        const std::string label = "Shadow " + std::to_string(index + 1);
        return GroupDecl{
            .id_ = std::string(ParamIds::kShadowGroupPrefix) + s,
            .slot_ = index,
            .label_ = label,
            .children_ = makeChildren(
                BooleanParameter::Spec{
                    .id_ = std::string(ParamIds::kShadowEnabledPrefix) + s,
                    .slot_ = static_cast<int>(ShadowSlot::kEnabled),
                    .label_ = "Enabled",
                    .default_ = false,
                },
                Double2DParameter::Spec{
                    .id_ = std::string(ParamIds::kShadowOffsetPrefix) + s,
                    .slot_ = static_cast<int>(ShadowSlot::kOffset),
                    .label_ = "Offset",
                    .default_ = {.x_ = 4.0, .y_ = -4.0},
                    .doubleType_ = OFX::eDoubleTypeXY,
                    .range_ =
                        std::pair{ParamPoint2D{.x_ = -500.0, .y_ = -500.0}, ParamPoint2D{.x_ = 500.0, .y_ = 500.0}},
                    .displayRange_ =
                        std::pair{ParamPoint2D{.x_ = -200.0, .y_ = -200.0}, ParamPoint2D{.x_ = 200.0, .y_ = 200.0}},
                    .increment_ = 1.0,
                },
                RGBAParameter::Spec{
                    .id_ = std::string(ParamIds::kShadowColorPrefix) + s,
                    .slot_ = static_cast<int>(ShadowSlot::kColor),
                    .label_ = "Color",
                    .default_ = {.r_ = 0.0F, .g_ = 0.0F, .b_ = 0.0F, .a_ = 0.8F},
                    .visibility_ =
                        VisibilityRule{
                            .logic_ = VisibilityRule::Logic::kAnd,
                            .conditions_ = {VisibilityCondition{
                                .paramId_ = std::string(ParamIds::kShadowEnabledPrefix) + s,
                                .expected_ = true,
                            }},
                        },
                }),
        };
    };

    auto onPage = [](const char* page) -> VisibilityRule {
        return VisibilityRule{
            .logic_ = VisibilityRule::Logic::kAnd,
            .conditions_ = {VisibilityCondition{.paramId_ = ParamIds::kPageSelect, .expected_ = std::string(page)}},
        };
    };

    return RootDecl{
        .children_ = makeChildren(

            // ---- Page selector (always visible, no visibility rule) ----
            ChoiceParameter::Spec{
                .id_ = ParamIds::kPageSelect,
                .label_ = "Page",
                .animates_ = false,
                .default_ = 0,
                .options_ = {ParamIds::kPageText, ParamIds::kPageLayout, ParamIds::kPageStyle},
            },

            // ================================================================
            // Text page
            // ================================================================
            GroupDecl{
                .id_ = ParamIds::kTextGroup,
                .label_ = "Text Settings",
                .open_ = true,
                .visibility_ = onPage(ParamIds::kPageText),
                .children_ = makeChildren(

                    StringParameter::Spec{
                        .id_ = ParamIds::kTextContent,
                        .label_ = "Text",
                        .animates_ = true,
                        .default_ = "Hello OFX",
                        .stringType_ = OFX::eStringTypeMultiLine,
                    },

                    ChoiceParameter::Spec{
                        .id_ = ParamIds::kFontName,
                        .label_ = "Font",
                        .animates_ = false,
                        .options_ = fontOptions,
                    },

                    DoubleParameter::Spec{
                        .id_ = ParamIds::kFontSize,
                        .label_ = "Font Size",
                        .default_ = 40.0,
                        .range_ = std::pair{1.0, 500.0},
                        .displayRange_ = std::pair{8.0, 200.0},
                        .increment_ = 1.0,
                    }),
            },

            // ================================================================
            // Layout page
            // ================================================================
            GroupDecl{
                .id_ = ParamIds::kLayoutGroup,
                .label_ = "Layout",
                .open_ = true,
                .visibility_ = onPage(ParamIds::kPageLayout),
                .children_ = makeChildren(

                    Double2DParameter::Spec{
                        .id_ = ParamIds::kPosition,
                        .label_ = "Position",
                        .default_ = {.x_ = 0.5, .y_ = 0.5},
                        .doubleType_ = OFX::eDoubleTypeXY,
                        .range_ = std::pair{ParamPoint2D{.x_ = -2.0, .y_ = -2.0}, ParamPoint2D{.x_ = 3.0, .y_ = 3.0}},
                        .displayRange_ =
                            std::pair{ParamPoint2D{.x_ = 0.0, .y_ = 0.0}, ParamPoint2D{.x_ = 1.0, .y_ = 1.0}},
                        .increment_ = 0.01,
                    },

                    DoubleParameter::Spec{
                        .id_ = ParamIds::kSize,
                        .label_ = "Size",
                        .default_ = 1.0,
                        .range_ = std::pair{0.01, 10.0},
                        .displayRange_ = std::pair{0.1, 4.0},
                        .increment_ = 0.01,
                    }),
            },

            // ================================================================
            // Style page
            // Demonstrates multi-condition secret switching:
            //   - opacity    : visible when pageSelect == "Style"  (single condition)
            //   - color      : visible when pageSelect == "Style" AND useCustomColor==true
            //                  (AND of two independent conditions)
            // ================================================================
            GroupDecl{
                .id_ = ParamIds::kStyleGroup,
                .label_ = "Style",
                .open_ = true,
                .visibility_ = onPage(ParamIds::kPageStyle),
                .children_ = makeChildren(

                    DoubleParameter::Spec{
                        .id_ = ParamIds::kOpacity,
                        .label_ = "Opacity",
                        .default_ = 1.0,
                        .range_ = std::pair{0.0, 1.0},
                        .displayRange_ = std::pair{0.0, 1.0},
                        .increment_ = 0.01,
                    },

                    BooleanParameter::Spec{
                        .id_ = ParamIds::kUseCustomColor,
                        .label_ = "Use Custom Color",
                        .default_ = false,
                    },

                    // Visible only when BOTH page==Style AND useCustomColor==true.
                    RGBAParameter::Spec{
                        .id_ = ParamIds::kColor,
                        .label_ = "Color",
                        .default_ = {.r_ = 1.0F, .g_ = 1.0F, .b_ = 1.0F, .a_ = 1.0F},
                        .visibility_ =
                            VisibilityRule{
                                .logic_ = VisibilityRule::Logic::kAnd,
                                .conditions_ =
                                    {
                                        VisibilityCondition{.paramId_ = ParamIds::kPageSelect,
                                                            .expected_ = std::string(ParamIds::kPageStyle)},
                                        VisibilityCondition{.paramId_ = ParamIds::kUseCustomColor, .expected_ = true},
                                    },
                            },
                    },

                    // Shadow slots — same structure repeated per slot index.
                    // Access: root_.findParameter<GroupParameter>("shadowGroup0")
                    //              ->getParameter<BooleanParameter>(ShadowSlot::kEnabled)
                    makeShadowGroup(0), makeShadowGroup(1)),
            }),
    };
}

// ---------------------------------------------------------------------------
// describe-time
// ---------------------------------------------------------------------------
void ParameterManager::defineAllParameters(OFX::ImageEffectDescriptor& descriptor) {
    defineTree(descriptor, makeDecl());
}

// ---------------------------------------------------------------------------
// instance-time
// ---------------------------------------------------------------------------
ParameterManager::ParameterManager(OFX::ImageEffect& effect) : effect_(&effect) {
    fetchTree(effect, makeDecl(), root_);
}

void ParameterManager::updateVisibility(double time) {
    root_.applyVisibilityRules();
}

// ---------------------------------------------------------------------------
// Value getters — one per value type
// ---------------------------------------------------------------------------
auto ParameterManager::getString(const std::string& id, double time, const std::string& fallback) const -> std::string {
    return root_.findValue<StringParameter>(id, time, fallback);
}

auto ParameterManager::getDouble(const std::string& id, double time, double fallback) const -> double {
    return root_.findValue<DoubleParameter>(id, time, fallback);
}

auto ParameterManager::getDouble2D(const std::string& id, double time, const ParamPoint2D& fallback) const
    -> ParamPoint2D {
    return root_.findValue<Double2DParameter>(id, time, fallback);
}

auto ParameterManager::getBool(const std::string& id, double time, bool fallback) const -> bool {
    return root_.findValue<BooleanParameter>(id, time, fallback);
}

auto ParameterManager::getChoice(const std::string& id, double time, int fallback) const -> int {
    return root_.findValue<ChoiceParameter>(id, time, fallback);
}

auto ParameterManager::getRGBA(const std::string& id, double time, const ParamRGBA& fallback) const -> ParamRGBA {
    return root_.findValue<RGBAParameter>(id, time, fallback);
}

auto ParameterManager::getChoiceOption(const std::string& id, double time, const std::string& fallback) const
    -> std::string {
    const auto* p = root_.findParameter<ChoiceParameter>(id);
    if (p == nullptr || p->options().empty()) {
        return fallback;
    }
    const int index = p->get(time);
    const auto& opts = p->options();
    return (index >= 0 && index < static_cast<int>(opts.size())) ? opts[index] : fallback;
}

auto ParameterManager::getSelectionMode(double /*time*/) const -> SelectionMode {
    return SelectionMode::Global;
}

void ParameterManager::setDouble(const std::string& id, double value, double time) {
    auto* p = root_.findParameter<DoubleParameter>(id);
    if (p != nullptr) {
        p->set(time, false, value);
    }
}

void ParameterManager::setDouble2D(const std::string& id, const ParamPoint2D& value, double time) {
    auto* p = root_.findParameter<Double2DParameter>(id);
    if (p != nullptr) {
        p->set(time, false, value);
    }
}

void ParameterManager::beginEditBlock(const std::string& name) {
    if (effect_ != nullptr) {
        effect_->beginEditBlock(name);
    }
}

void ParameterManager::endEditBlock() {
    if (effect_ != nullptr) {
        effect_->endEditBlock();
    }
}

}  // namespace MugLab::OfxBase
