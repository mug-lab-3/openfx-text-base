#include "GroupParameter.h"

namespace MugLab::OfxBase {

GroupParameter::GroupParameter(Spec spec) : ParameterNode(spec.id_, spec.slot_), spec_(std::move(spec)) {
}

auto GroupParameter::create(Spec spec) -> std::unique_ptr<GroupParameter> {
    return std::make_unique<GroupParameter>(std::move(spec));
}

auto GroupParameter::addChild(std::unique_ptr<ParameterNode> child) -> GroupParameter& {
    childById_[child->id()] = child.get();
    if (const auto s = child->slot()) {
        childBySlot_[*s] = child.get();
    }
    children_.push_back(std::move(child));
    return *this;
}

void GroupParameter::define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) {
    auto* groupDescriptor = descriptor.defineGroupParam(id());
    if (!spec_.label_.empty() || !spec_.shortLabel_.empty() || !spec_.longLabel_.empty()) {
        const auto& shortLabel = spec_.shortLabel_.empty() ? spec_.label_ : spec_.shortLabel_;
        const auto& longLabel = spec_.longLabel_.empty() ? spec_.label_ : spec_.longLabel_;
        groupDescriptor->setLabels(spec_.label_, shortLabel, longLabel);
    }
    if (!spec_.hint_.empty()) {
        groupDescriptor->setHint(spec_.hint_);
    }
    groupDescriptor->setOpen(spec_.open_);
    if (parentGroup != nullptr) {
        groupDescriptor->setParent(*parentGroup);
    }

    for (auto& child : children_) {
        child->define(descriptor, groupDescriptor);
    }
}

void GroupParameter::fetch(OFX::ImageEffect& effectInstance) {
    group_ = effectInstance.fetchGroupParam(id());
    for (auto& child : children_) {
        child->fetch(effectInstance);
    }
}

auto GroupParameter::children() const -> const std::vector<std::unique_ptr<ParameterNode>>& {
    return children_;
}

auto GroupParameter::group() const -> OFX::GroupParam* {
    return group_;
}

void GroupParameter::reset(bool deleteAnimation, double time) {
    for (auto& child : children_) {
        child->reset(deleteAnimation, time);
    }
}

void GroupParameter::deleteKeysInRange(double lo, double hi) {
    for (auto& child : children_) {
        child->deleteKeysInRange(lo, hi);
    }
}

void GroupParameter::setKeyFrameAtTime(double time) {
    for (auto& child : children_) {
        child->setKeyFrameAtTime(time);
    }
}

void GroupParameter::applyVisible(bool visible) {
    if (group_ != nullptr) {
        group_->setIsSecret(!visible);
    }
}

}  // namespace MugLab::OfxBase
