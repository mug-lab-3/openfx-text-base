#include "StringParameter.h"

namespace MugLab::OfxBase {

template <>
auto ParameterBase<OFX::StringParam, std::string>::fetchFrom(OFX::ImageEffect& effectInstance) -> OFX::StringParam* {
    return effectInstance.fetchStringParam(id());
}

StringParameter::StringParameter(Spec spec) : ParameterBase(spec.id_, spec.slot_), spec_(std::move(spec)) {
}

void StringParameter::define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) {
    auto* paramDescriptor = descriptor.defineStringParam(id());
    applyBaseSpec(*paramDescriptor, spec_, parentGroup);
    paramDescriptor->setDefault(spec_.default_);
    if (spec_.stringType_ != OFX::eStringTypeSingleLine) {
        paramDescriptor->setStringType(spec_.stringType_);
    }
    if (!spec_.visible_) {
        paramDescriptor->setIsSecret(true);
    }
}

auto StringParameter::get(double time) const -> std::string {
    std::string result = spec_.default_;
    if (raw_ != nullptr) {
        std::string value;
        if (spec_.animates_) {
            raw_->getValueAtTime(time, value);
        } else {
            raw_->getValue(value);
        }
        result = value;
    }
    return result;
}

void StringParameter::setKeyFrameAtTimeImpl(double time) {
    if (raw_ == nullptr || !spec_.animates_) {
        return;
    }
    std::string value;
    raw_->getValueAtTime(time, value);
    raw_->setValueAtTime(time, value);
}

void StringParameter::resetToDefault(double time) {
    if (raw_ == nullptr) {
        return;
    }
    if (spec_.animates_ && raw_->getNumKeys() > 0) {
        raw_->setValueAtTime(time, spec_.default_);
    } else {
        raw_->setValue(spec_.default_);
    }
}

void StringParameter::set(double time, bool forceKeyFrame, const std::string& value) {
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
