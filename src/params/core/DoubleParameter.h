#pragma once

#include <string>
#include <utility>

#include "ParameterBase.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

class DoubleParameter : public ParameterBase<OFX::DoubleParam, double> {
   public:
    struct Spec {
        std::string id_;
        std::optional<int> slot_;
        std::string label_;
        std::string shortLabel_;
        std::string longLabel_;
        std::string hint_;
        bool animates_ = true;
        double default_ = 0.0;
        bool visible_ = true;
        std::optional<VisibilityRule> visibility_;
        OFX::DoubleTypeEnum doubleType_ = OFX::eDoubleTypePlain;
        std::optional<std::pair<double, double>> range_;
        std::optional<std::pair<double, double>> displayRange_;
        double increment_ = 0.01;
    };

    explicit DoubleParameter(Spec spec);

    void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) override;

    [[nodiscard]] auto get(double time) const -> double;
    void set(double time, bool forceKeyFrame, double value);

   protected:
    void resetToDefault(double time) override;
    void setKeyFrameAtTimeImpl(double time) override;

   private:
    Spec spec_;
};

}  // namespace MugLab::OfxBase
