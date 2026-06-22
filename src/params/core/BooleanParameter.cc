#include "BooleanParameter.h"

namespace MugLab::OfxBase {

template <>
auto ParameterBase<OFX::BooleanParam, bool>::fetchFrom(OFX::ImageEffect& effectInstance) -> OFX::BooleanParam* {
    return effectInstance.fetchBooleanParam(id());
}

BooleanParameter::BooleanParameter(Spec spec) : ParameterBase(spec.id_, spec.slot_), spec_(std::move(spec)) {
}

void BooleanParameter::define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) {
    auto* paramDescriptor = descriptor.defineBooleanParam(id());
    applyBaseSpec(*paramDescriptor, spec_, parentGroup);
    paramDescriptor->setDefault(spec_.default_);
    if (!spec_.visible_) {
        paramDescriptor->setIsSecret(true);
    }
}

auto BooleanParameter::get(double time) const -> bool {
    bool result = spec_.default_;
    if (raw_ != nullptr) {
        bool value = false;
        if (spec_.animates_) {
            raw_->getValueAtTime(time, value);
        } else {
            raw_->getValue(value);
        }
        result = value;
    }
    return result;
}

void BooleanParameter::setKeyFrameAtTimeImpl(double time) {
    if (raw_ == nullptr || !spec_.animates_) {
        return;
    }
    bool value = false;
    raw_->getValueAtTime(time, value);
    raw_->setValueAtTime(time, value);
}

void BooleanParameter::resetToDefault(double time) {
    if (raw_ == nullptr) {
        return;
    }
    if (spec_.animates_ && raw_->getNumKeys() > 0) {
        raw_->setValueAtTime(time, spec_.default_);
    } else {
        raw_->setValue(spec_.default_);
    }
}

void BooleanParameter::set(double time, bool forceKeyFrame, bool value) {
    if (raw_ == nullptr || !isFiniteValue(time)) {
        return;
    }
    if (spec_.animates_ && (forceKeyFrame || raw_->getNumKeys() > 0)) {
        raw_->setValueAtTime(time, value);
    } else {
        raw_->setValue(value);
    }
}

}  // namespace MugLab::OfxBase
