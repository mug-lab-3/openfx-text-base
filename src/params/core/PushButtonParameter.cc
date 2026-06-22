#include "PushButtonParameter.h"

namespace MugLab::OfxBase {

PushButtonParameter::PushButtonParameter(Spec spec) : ParameterNode(spec.id_, spec.slot_), spec_(std::move(spec)) {
}

void PushButtonParameter::define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) {
    auto* paramDescriptor = descriptor.definePushButtonParam(id());
    if (!spec_.label_.empty() || !spec_.shortLabel_.empty() || !spec_.longLabel_.empty()) {
        const auto& shortLabel = spec_.shortLabel_.empty() ? spec_.label_ : spec_.shortLabel_;
        const auto& longLabel = spec_.longLabel_.empty() ? spec_.label_ : spec_.longLabel_;
        paramDescriptor->setLabels(spec_.label_, shortLabel, longLabel);
    }
    if (!spec_.hint_.empty()) {
        paramDescriptor->setHint(spec_.hint_);
    }
    if (parentGroup != nullptr) {
        paramDescriptor->setParent(*parentGroup);
    }
    if (!spec_.visible_) {
        paramDescriptor->setIsSecret(true);
    }
}

void PushButtonParameter::fetch(OFX::ImageEffect& effectInstance) {
    raw_ = effectInstance.fetchPushButtonParam(id());
}

void PushButtonParameter::applyVisible(bool visible) {
    if (raw_ != nullptr) {
        raw_->setIsSecret(!visible);
    }
}

}  // namespace MugLab::OfxBase
