#pragma once

#include <string>

#include "ParameterBase.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

class BooleanParameter : public ParameterBase<OFX::BooleanParam, bool> {
   public:
    struct Spec {
        std::string id_;
        std::optional<int> slot_;
        std::string label_;
        std::string shortLabel_;
        std::string longLabel_;
        std::string hint_;
        bool animates_ = true;
        bool default_ = false;
        bool visible_ = true;
        std::optional<VisibilityRule> visibility_;
    };

    explicit BooleanParameter(Spec spec);

    void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) override;

    [[nodiscard]] auto get(double time) const -> bool;
    void set(double time, bool forceKeyFrame, bool value);

   protected:
    void resetToDefault(double time) override;
    void setKeyFrameAtTimeImpl(double time) override;

   private:
    Spec spec_;
};

}  // namespace MugLab::OfxBase
