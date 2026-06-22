#include "IntParameter.h"

#include <algorithm>

namespace MugLab::OfxBase {

template <>
auto ParameterBase<OFX::IntParam, int>::fetchFrom(OFX::ImageEffect& effectInstance) -> OFX::IntParam* {
    return effectInstance.fetchIntParam(id());
}

IntParameter::IntParameter(Spec spec) : ParameterBase(spec.id_, spec.slot_), spec_(std::move(spec)) {
}

void IntParameter::define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) {
    auto* paramDescriptor = descriptor.defineIntParam(id());
    applyBaseSpec(*paramDescriptor, spec_, parentGroup);
    paramDescriptor->setDefault(spec_.default_);
    if (spec_.range_) {
        paramDescriptor->setRange(spec_.range_->first, spec_.range_->second);
    }
    if (spec_.displayRange_) {
        paramDescriptor->setDisplayRange(spec_.displayRange_->first, spec_.displayRange_->second);
    }
    if (!spec_.visible_) {
        paramDescriptor->setIsSecret(true);
    }
}

auto IntParameter::get(double time) const -> int {
    int result = spec_.default_;
    if (raw_ != nullptr) {
        int value = 0;
        if (spec_.animates_) {
            raw_->getValueAtTime(time, value);
        } else {
            raw_->getValue(value);
        }
        result = value;
    }
    return result;
}

void IntParameter::setKeyFrameAtTimeImpl(double time) {
    if (raw_ == nullptr || !spec_.animates_) {
        return;
    }
    int value = 0;
    raw_->getValueAtTime(time, value);
    raw_->setValueAtTime(time, value);
}

void IntParameter::resetToDefault(double time) {
    if (raw_ == nullptr) {
        return;
    }
    if (spec_.animates_ && raw_->getNumKeys() > 0) {
        raw_->setValueAtTime(time, spec_.default_);
    } else {
        raw_->setValue(spec_.default_);
    }
}

void IntParameter::set(double time, bool forceKeyFrame, int value) {
    if (raw_ == nullptr || !isFiniteValue(time)) {
        return;
    }
    int clamped = spec_.range_ ? std::clamp(value, spec_.range_->first, spec_.range_->second) : value;
    if (spec_.animates_ && (forceKeyFrame || raw_->getNumKeys() > 0)) {
        raw_->setValueAtTime(time, clamped);
    } else {
        raw_->setValue(clamped);
    }
}

}  // namespace MugLab::OfxBase
