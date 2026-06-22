#include "ChoiceParameter.h"

namespace MugLab::OfxBase {

template <>
auto ParameterBase<OFX::ChoiceParam, int>::fetchFrom(OFX::ImageEffect& effectInstance) -> OFX::ChoiceParam* {
    return effectInstance.fetchChoiceParam(id());
}

ChoiceParameter::ChoiceParameter(Spec spec) : ParameterBase(spec.id_, spec.slot_), spec_(std::move(spec)) {
}

void ChoiceParameter::define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) {
    auto* paramDescriptor = descriptor.defineChoiceParam(id());
    applyBaseSpec(*paramDescriptor, spec_, parentGroup);
    for (const auto& option : spec_.options_) {
        paramDescriptor->appendOption(option);
    }
    paramDescriptor->setDefault(spec_.default_);
    if (!spec_.visible_) {
        paramDescriptor->setIsSecret(true);
    }
}

auto ChoiceParameter::get(double time) const -> int {
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

void ChoiceParameter::setKeyFrameAtTimeImpl(double time) {
    if (raw_ == nullptr || !spec_.animates_) {
        return;
    }
    int value = 0;
    raw_->getValueAtTime(time, value);
    raw_->setValueAtTime(time, value);
}

void ChoiceParameter::resetToDefault(double time) {
    if (raw_ == nullptr) {
        return;
    }
    if (spec_.animates_ && raw_->getNumKeys() > 0) {
        raw_->setValueAtTime(time, spec_.default_);
    } else {
        raw_->setValue(spec_.default_);
    }
}

void ChoiceParameter::set(double time, bool forceKeyFrame, int value) {
    if (raw_ == nullptr || !isFiniteValue(time)) {
        return;
    }
    // Bounds check to ensure the value falls within valid options.
    if (value < 0 || value >= static_cast<int>(spec_.options_.size())) {
        return;
    }
    if (spec_.animates_ && (forceKeyFrame || raw_->getNumKeys() > 0)) {
        raw_->setValueAtTime(time, value);
    } else {
        raw_->setValue(value);
    }
}

void ChoiceParameter::set(double time, bool forceKeyFrame, const std::string& optionName) {
    if (raw_ == nullptr) {
        return;
    }
    for (size_t index = 0; index < spec_.options_.size(); ++index) {
        if (spec_.options_[index] == optionName) {
            set(time, forceKeyFrame, static_cast<int>(index));
            break;
        }
    }
}

}  // namespace MugLab::OfxBase
