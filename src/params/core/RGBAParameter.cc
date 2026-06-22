#include "RGBAParameter.h"

#include <algorithm>
#include <vector>

namespace MugLab::OfxBase {

template <>
auto ParameterBase<OFX::RGBAParam, ParamRGBA>::fetchFrom(OFX::ImageEffect& effectInstance) -> OFX::RGBAParam* {
    return effectInstance.fetchRGBAParam(id());
}

RGBAParameter::RGBAParameter(Spec spec) : ParameterBase(spec.id_, spec.slot_), spec_(std::move(spec)) {
}

void RGBAParameter::define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) {
    auto* paramDescriptor = descriptor.defineRGBAParam(id());
    applyBaseSpec(*paramDescriptor, spec_, parentGroup);
    paramDescriptor->setDefault(spec_.default_.r_, spec_.default_.g_, spec_.default_.b_, spec_.default_.a_);
    if (!spec_.visible_) {
        paramDescriptor->setIsSecret(true);
    }
}

auto RGBAParameter::get(double time) const -> ParamRGBA {
    ParamRGBA result = spec_.default_;
    if (raw_ != nullptr) {
        double r = 0.0;
        double g = 0.0;
        double b = 0.0;
        double a = 0.0;
        if (spec_.animates_) {
            raw_->getValueAtTime(time, r, g, b, a);
        } else {
            raw_->getValue(r, g, b, a);
        }
        result = ParamRGBA{.r_ = static_cast<float>(std::clamp(r, 0.0, 1.0)),
                           .g_ = static_cast<float>(std::clamp(g, 0.0, 1.0)),
                           .b_ = static_cast<float>(std::clamp(b, 0.0, 1.0)),
                           .a_ = static_cast<float>(std::clamp(a, 0.0, 1.0))};
    }
    return result;
}

void RGBAParameter::setKeyFrameAtTimeImpl(double time) {
    if (raw_ == nullptr || !spec_.animates_) {
        return;
    }
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 0.0;
    raw_->getValueAtTime(time, r, g, b, a);
    raw_->setValueAtTime(time, r, g, b, a);
}

void RGBAParameter::deleteKeysInRange(double lo, double hi) {
    if (raw_ == nullptr) {
        return;
    }
    const unsigned int numKeys = raw_->getNumKeys();
    std::vector<double> toDelete;
    toDelete.reserve(numKeys);
    for (unsigned int i = 0; i < numKeys; ++i) {
        const double t = raw_->getKeyTime(static_cast<int>(i));
        if (t >= lo && t <= hi) {
            toDelete.push_back(t);
        }
    }
    // Some hosts only remove one channel's keyframe per paramDeleteKey call on a
    // multi-component param, so repeat per time until no key remains there.
    constexpr int kMaxComponents = 4;
    for (const double t : toDelete) {
        for (int i = 0; i < kMaxComponents && raw_->getKeyIndex(t, OFX::eKeySearchNear) != -1; ++i) {
            raw_->deleteKeyAtTime(t);
        }
    }
}

void RGBAParameter::resetToDefault(double time) {
    if (raw_ == nullptr) {
        return;
    }
    if (spec_.animates_ && raw_->getNumKeys() > 0) {
        raw_->setValueAtTime(time, spec_.default_.r_, spec_.default_.g_, spec_.default_.b_, spec_.default_.a_);
    } else {
        raw_->setValue(spec_.default_.r_, spec_.default_.g_, spec_.default_.b_, spec_.default_.a_);
    }
}

void RGBAParameter::set(double time, bool forceKeyFrame, const ParamRGBA& value) {
    if (raw_ == nullptr || !isFiniteValue(time) || !isFiniteValue(value.r_) || !isFiniteValue(value.g_) ||
        !isFiniteValue(value.b_) || !isFiniteValue(value.a_)) {
        return;
    }
    if (spec_.animates_ && (forceKeyFrame || raw_->getNumKeys() > 0)) {
        raw_->setValueAtTime(time, value.r_, value.g_, value.b_, value.a_);
    } else {
        raw_->setValue(value.r_, value.g_, value.b_, value.a_);
    }
}

}  // namespace MugLab::OfxBase
