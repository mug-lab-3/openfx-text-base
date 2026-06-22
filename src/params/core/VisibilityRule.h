#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace MugLab::OfxBase {

// A single trigger for visibility, evaluated against a Boolean or Choice parameter.
// For BooleanParameter: visible = isAnimating || (getValue() == bool)
// For ChoiceParameter:  visible = isAnimating || (getValue() == index)
//   where index is the int directly, or the position of the string in the target's options_.
// NOTE: when using a string option name, wrap it in std::string(...) — a bare
// const char* literal is ambiguous against the bool alternative and won't compile.
struct VisibilityCondition {
    std::string paramId_;
    std::variant<bool, int, std::string> expected_ = true;
};

// Visibility rule composed of one or more VisibilityCondition.
// Logic::kAnd: all conditions must be satisfied.
// Logic::kOr:  at least one condition must be satisfied.
struct VisibilityRule {
    enum class Logic : uint8_t {
        kAnd,
        kOr
    } logic_ = Logic::kAnd;
    std::vector<VisibilityCondition> conditions_;
};

}  // namespace MugLab::OfxBase
