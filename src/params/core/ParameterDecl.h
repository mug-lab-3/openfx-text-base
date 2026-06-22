#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "BooleanParameter.h"
#include "ChoiceParameter.h"
#include "Double2DParameter.h"
#include "DoubleParameter.h"
#include "IntParameter.h"
#include "PushButtonParameter.h"
#include "RGBAParameter.h"
#include "StringParameter.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

// Forward declaration for recursive GroupDecl.
struct GroupDecl;

// A single node in a declarative parameter tree. Leaf nodes reuse the
// existing XxxParameter::Spec types directly; GroupDecl carries the same
// fields as GroupParameter::Spec plus a children list.
using ParameterDecl =
    std::variant<BooleanParameter::Spec, ChoiceParameter::Spec, Double2DParameter::Spec, DoubleParameter::Spec,
                 IntParameter::Spec, PushButtonParameter::Spec, RGBAParameter::Spec, StringParameter::Spec, GroupDecl>;

// Declarative group node. Mirrors GroupParameter::Spec fields directly
// (no inheritance) so designated initializers work across the whole tree.
// children_ uses unique_ptr to break the recursive type dependency.
struct GroupDecl {
    std::string id_;
    std::optional<int> slot_;
    std::string label_;
    std::string shortLabel_;
    std::string longLabel_;
    std::string hint_;
    bool open_ = true;
    std::optional<VisibilityRule> visibility_;
    std::vector<std::unique_ptr<ParameterDecl>> children_;
};

// Root-level declarative container. Unlike GroupDecl, no OFX GroupParam is created —
// children are registered directly as top-level params. Used with RootParameterGroup.
struct RootDecl {
    std::vector<std::unique_ptr<ParameterDecl>> children_;
};

// Helper to build a ParameterDecl child list with a readable syntax:
//
//   makeChildren(
//       RGBAParameter::Spec{...},
//       BooleanParameter::Spec{...},
//       GroupDecl{...}
//   )
template <typename... Ts>
auto makeChildren(Ts&&... decls) -> std::vector<std::unique_ptr<ParameterDecl>> {
    std::vector<std::unique_ptr<ParameterDecl>> result;
    result.reserve(sizeof...(Ts));
    (result.push_back(std::make_unique<ParameterDecl>(std::forward<Ts>(decls))), ...);
    return result;
}

}  // namespace MugLab::OfxBase
