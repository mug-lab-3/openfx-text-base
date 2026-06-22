#pragma once

#include <string>

#include "ParameterBase.h"
#include "Types.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

class RGBAParameter : public ParameterBase<OFX::RGBAParam, ParamRGBA> {
   public:
    struct Spec {
        std::string id_;
        std::optional<int> slot_;
        std::string label_;
        std::string shortLabel_;
        std::string longLabel_;
        std::string hint_;
        bool animates_ = true;
        ParamRGBA default_{.r_ = 0.0F, .g_ = 0.0F, .b_ = 0.0F, .a_ = 1.0F};
        bool visible_ = true;
        std::optional<VisibilityRule> visibility_;
    };

    explicit RGBAParameter(Spec spec);

    void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) override;

    [[nodiscard]] auto get(double time) const -> ParamRGBA;
    void set(double time, bool forceKeyFrame, const ParamRGBA& value);

    void deleteKeysInRange(double lo, double hi) override;

   protected:
    void resetToDefault(double time) override;
    void setKeyFrameAtTimeImpl(double time) override;

   private:
    Spec spec_;
};

}  // namespace MugLab::OfxBase
