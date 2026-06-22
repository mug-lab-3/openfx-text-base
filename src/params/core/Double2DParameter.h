#pragma once

#include <string>
#include <utility>

#include "ParameterBase.h"
#include "Types.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

class Double2DParameter : public ParameterBase<OFX::Double2DParam, ParamPoint2D> {
   public:
    struct Spec {
        std::string id_;
        std::optional<int> slot_;
        std::string label_;
        std::string shortLabel_;
        std::string longLabel_;
        std::string hint_;
        bool animates_ = true;
        ParamPoint2D default_{.x_ = 0.0, .y_ = 0.0};
        bool visible_ = true;
        std::optional<VisibilityRule> visibility_;
        OFX::DoubleTypeEnum doubleType_ = OFX::eDoubleTypePlain;
        std::optional<std::pair<ParamPoint2D, ParamPoint2D>> range_;
        std::optional<std::pair<ParamPoint2D, ParamPoint2D>> displayRange_;
        double increment_ = 0.01;
    };

    explicit Double2DParameter(Spec spec);

    void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) override;

    [[nodiscard]] auto get(double time) const -> ParamPoint2D;
    void set(double time, bool forceKeyFrame, const ParamPoint2D& value);
    void setX(double time, bool forceKeyFrame, double valueX);
    void setY(double time, bool forceKeyFrame, double valueY);

   protected:
    void resetToDefault(double time) override;
    void setKeyFrameAtTimeImpl(double time) override;

   private:
    Spec spec_;
};

}  // namespace MugLab::OfxBase
