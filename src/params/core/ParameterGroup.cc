#include "ParameterGroup.h"

namespace MugLab::OfxBase {

void ParameterGroup::setTree(std::unique_ptr<GroupParameter> root) {
    indexNode(*root);
    ownedRoot_ = std::move(root);
    resolveVisibilityRules();
}

auto ParameterGroup::root() const -> GroupParameter* {
    return ownedRoot_.get();
}

}  // namespace MugLab::OfxBase
