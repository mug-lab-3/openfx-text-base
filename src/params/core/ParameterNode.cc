#include "ParameterNode.h"

namespace MugLab::OfxBase {

ParameterNode::ParameterNode(std::string id, std::optional<int> slot) : id_(std::move(id)), slot_(slot) {
}

auto ParameterNode::id() const -> const std::string& {
    return id_;
}

auto ParameterNode::slot() const -> std::optional<int> {
    return slot_;
}

void ParameterNode::setVisible(bool visible) {
    if (lastVisible_.has_value() && visible == *lastVisible_) {
        return;
    }
    applyVisible(visible);
    lastVisible_ = visible;
}

}  // namespace MugLab::OfxBase
