#include "Double2DParameter.h"

#include <algorithm>

namespace MugLab::OfxBase {

template <>
auto ParameterBase<OFX::Double2DParam, ParamPoint2D>::fetchFrom(OFX::ImageEffect& effectInstance)
    -> OFX::Double2DParam* {
    return effectInstance.fetchDouble2DParam(id());
}

Double2DParameter::Double2DParameter(Spec spec) : ParameterBase(spec.id_, spec.slot_), spec_(std::move(spec)) {
}

void Double2DParameter::define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) {
    auto* paramDescriptor = descriptor.defineDouble2DParam(id());
    applyBaseSpec(*paramDescriptor, spec_, parentGroup);
    paramDescriptor->setDoubleType(spec_.doubleType_);
    paramDescriptor->setDefault(spec_.default_.x_, spec_.default_.y_);
    if (spec_.range_) {
        paramDescriptor->setRange(spec_.range_->first.x_, spec_.range_->first.y_, spec_.range_->second.x_,
                                  spec_.range_->second.y_);
    }
    if (spec_.displayRange_) {
        paramDescriptor->setDisplayRange(spec_.displayRange_->first.x_, spec_.displayRange_->first.y_,
                                         spec_.displayRange_->second.x_, spec_.displayRange_->second.y_);
    }
    paramDescriptor->setIncrement(spec_.increment_);
    if (!spec_.visible_) {
        paramDescriptor->setIsSecret(true);
    }
}

auto Double2DParameter::get(double time) const -> ParamPoint2D {
    ParamPoint2D result = spec_.default_;
    if (raw_ != nullptr) {
        double x = 0.0;
        double y = 0.0;
        if (spec_.animates_) {
            raw_->getValueAtTime(time, x, y);
        } else {
            raw_->getValue(x, y);
        }
        result = ParamPoint2D{.x_ = x, .y_ = y};
    }
    if (spec_.range_) {
        result = ParamPoint2D{.x_ = std::clamp(result.x_, spec_.range_->first.x_, spec_.range_->second.x_),
                              .y_ = std::clamp(result.y_, spec_.range_->first.y_, spec_.range_->second.y_)};
    }
    return result;
}

void Double2DParameter::setKeyFrameAtTimeImpl(double time) {
    if (raw_ == nullptr || !spec_.animates_) {
        return;
    }
    double x = 0.0;
    double y = 0.0;
    raw_->getValueAtTime(time, x, y);
    raw_->setValueAtTime(time, x, y);
}

void Double2DParameter::resetToDefault(double time) {
    if (raw_ == nullptr) {
        return;
    }
    if (spec_.animates_ && raw_->getNumKeys() > 0) {
        raw_->setValueAtTime(time, spec_.default_.x_, spec_.default_.y_);
    } else {
        raw_->setValue(spec_.default_.x_, spec_.default_.y_);
    }
}

void Double2DParameter::set(double time, bool forceKeyFrame, const ParamPoint2D& value) {
    if (raw_ == nullptr || !isFiniteValue(time) || !isFiniteValue(value.x_) || !isFiniteValue(value.y_)) {
        return;
    }
    ParamPoint2D clamped = value;
    if (spec_.range_) {
        clamped = ParamPoint2D{.x_ = std::clamp(value.x_, spec_.range_->first.x_, spec_.range_->second.x_),
                               .y_ = std::clamp(value.y_, spec_.range_->first.y_, spec_.range_->second.y_)};
    }
    if (spec_.animates_ && (forceKeyFrame || raw_->getNumKeys() > 0)) {
        raw_->setValueAtTime(time, clamped.x_, clamped.y_);
    } else {
        raw_->setValue(clamped.x_, clamped.y_);
    }
}

void Double2DParameter::setX(double time, bool forceKeyFrame, double valueX) {
    ParamPoint2D current = get(time);
    current.x_ = valueX;
    set(time, forceKeyFrame, current);
}

void Double2DParameter::setY(double time, bool forceKeyFrame, double valueY) {
    ParamPoint2D current = get(time);
    current.y_ = valueY;
    set(time, forceKeyFrame, current);
}

}  // namespace MugLab::OfxBase
