#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "ParameterNode.h"

namespace MugLab::OfxBase {

// Thin template layer that only absorbs the per-OFX-type differences needed to
// hold and fetch the raw parameter pointer. All value access (get/set/clamp/
// conversion) lives in the non-template concrete classes derived from this, so
// that their implementations can live in .cc files instead of headers.
template <typename OfxParamT, typename ValueT>
class ParameterBase : public ParameterNode {
   public:
    using ValueType = ValueT;

    explicit ParameterBase(std::string id, std::optional<int> slot = std::nullopt)
        : ParameterNode(std::move(id), slot) {
    }

    void fetch(OFX::ImageEffect& effectInstance) override {
        raw_ = fetchFrom(effectInstance);
    }

    [[nodiscard]] auto raw() const -> OfxParamT* {
        return raw_;
    }

    [[nodiscard]] auto rawValueParam() const -> OFX::ValueParam* override {
        return static_cast<OFX::ValueParam*>(raw_);
    }

    [[nodiscard]] auto isAnimating() const -> bool override {
        return (raw_ != nullptr) && (raw_->getNumKeys() > 0);
    }

    void reset(bool deleteAnimation, double time) override {
        if (raw_ == nullptr) {
            return;
        }
        if (deleteAnimation) {
            raw_->deleteAllKeys();
        }
        resetToDefault(time);
    }

    void setKeyFrameAtTime(double time) override {
        if (raw_ == nullptr) {
            return;
        }
        setKeyFrameAtTimeImpl(time);
    }

    void deleteKeysInRange(double lo, double hi) override {
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
        for (const double t : toDelete) {
            raw_->deleteKeyAtTime(t);
        }
    }

   protected:
    virtual void resetToDefault(double time) = 0;
    virtual void setKeyFrameAtTimeImpl(double time) = 0;

    void applyVisible(bool visible) override {
        if (raw_ != nullptr) {
            raw_->setIsSecret(!visible);
        }
    }

    OfxParamT* raw_ = nullptr;

   private:
    [[nodiscard]] auto fetchFrom(OFX::ImageEffect& effectInstance) -> OfxParamT*;
};

inline auto isFiniteValue(double value) -> bool {
    return std::isfinite(value);
}

}  // namespace MugLab::OfxBase
