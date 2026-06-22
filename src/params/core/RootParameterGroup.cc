#include "RootParameterGroup.h"

#include "BooleanParameter.h"
#include "ChoiceParameter.h"
#include "Double2DParameter.h"
#include "DoubleParameter.h"
#include "GroupParameter.h"
#include "RGBAParameter.h"

namespace MugLab::OfxBase {

static void collectAnimatingInfoFromNode(const ParameterNode& node, std::vector<ParamIdTypePair>& out) {
    if (const auto* group = dynamic_cast<const GroupParameter*>(&node)) {
        for (const auto& child : group->children()) {
            collectAnimatingInfoFromNode(*child, out);
        }
    } else if (dynamic_cast<const DoubleParameter*>(&node) != nullptr) {
        if (node.isAnimating()) {
            out.push_back({.id_ = node.id(), .type_ = ParamValueType::Double});
        }
    } else if (dynamic_cast<const Double2DParameter*>(&node) != nullptr) {
        if (node.isAnimating()) {
            out.push_back({.id_ = node.id(), .type_ = ParamValueType::Double2D});
        }
    } else if (dynamic_cast<const RGBAParameter*>(&node) != nullptr) {
        if (node.isAnimating()) {
            out.push_back({.id_ = node.id(), .type_ = ParamValueType::RGBA});
        }
    } else if (dynamic_cast<const ChoiceParameter*>(&node) != nullptr) {
        if (node.isAnimating()) {
            out.push_back({.id_ = node.id(), .type_ = ParamValueType::Choice});
        }
    } else if (dynamic_cast<const BooleanParameter*>(&node) != nullptr) {
        if (node.isAnimating()) {
            out.push_back({.id_ = node.id(), .type_ = ParamValueType::Boolean});
        }
    }
}

auto RootParameterGroup::findValueParam(const std::string& id) const -> OFX::ValueParam* {
    const auto* node = findNode(id);
    return node != nullptr ? node->rawValueParam() : nullptr;
}

auto RootParameterGroup::collectAnimatingParamInfos() const -> std::vector<ParamIdTypePair> {
    std::vector<ParamIdTypePair> result;
    for (const auto& child : children_) {
        collectAnimatingInfoFromNode(*child, result);
    }
    return result;
}

void RootParameterGroup::addChild(std::unique_ptr<ParameterNode> child) {
    indexNode(*child);
    children_.push_back(std::move(child));
    resolveVisibilityRules();
}

}  // namespace MugLab::OfxBase
