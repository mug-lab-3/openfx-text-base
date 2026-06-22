#include "DoubleParameter.h"

#include <algorithm>

namespace MugLab::OfxBase {

template <>
auto ParameterBase<OFX::DoubleParam, double>::fetchFrom(OFX::ImageEffect& effectInstance) -> OFX::DoubleParam* {
    return effectInstance.fetchDoubleParam(id());
}

DoubleParameter::DoubleParameter(Spec spec) : ParameterBase(spec.id_, spec.slot_), spec_(std::move(spec)) {
}

void DoubleParameter::define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) {
    auto* paramDescriptor = descriptor.defineDoubleParam(id());
    applyBaseSpec(*paramDescriptor, spec_, parentGroup);
    paramDescriptor->setDoubleType(spec_.doubleType_);
    paramDescriptor->setDefault(spec_.default_);
    if (spec_.range_) {
        paramDescriptor->setRange(spec_.range_->first, spec_.range_->second);
    }
    if (spec_.displayRange_) {
        paramDescriptor->setDisplayRange(spec_.displayRange_->first, spec_.displayRange_->second);
    }
    paramDescriptor->setIncrement(spec_.increment_);
    if (!spec_.visible_) {
        paramDescriptor->setIsSecret(true);
    }
}

auto DoubleParameter::get(double time) const -> double {
    double result = spec_.default_;
    if (raw_ != nullptr) {
        double value = 0.0;
        if (spec_.animates_) {
            raw_->getValueAtTime(time, value);
        } else {
            raw_->getValue(value);
        }
        result = value;
    }
    if (spec_.range_) {
        result = std::clamp(result, spec_.range_->first, spec_.range_->second);
    }
    return result;
}

void DoubleParameter::setKeyFrameAtTimeImpl(double time) {
    if (raw_ == nullptr || !spec_.animates_) {
        return;
    }
    double value = 0.0;
    raw_->getValueAtTime(time, value);
    raw_->setValueAtTime(time, value);
}

void DoubleParameter::resetToDefault(double time) {
    if (raw_ == nullptr) {
        return;
    }
    if (spec_.animates_ && raw_->getNumKeys() > 0) {
        raw_->setValueAtTime(time, spec_.default_);
    } else {
        raw_->setValue(spec_.default_);
    }
}

void DoubleParameter::set(double time, bool forceKeyFrame, double value) {
    if (raw_ == nullptr || !isFiniteValue(time) || !isFiniteValue(value)) {
        return;
    }
    double clamped = value;
    if (spec_.range_) {
        clamped = std::clamp(clamped, spec_.range_->first, spec_.range_->second);
    }
    if (spec_.animates_ && (forceKeyFrame || raw_->getNumKeys() > 0)) {
        raw_->setValueAtTime(time, clamped);
    } else {
        raw_->setValue(clamped);
    }
}

}  // namespace MugLab::OfxBase
