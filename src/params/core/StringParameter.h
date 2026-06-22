#pragma once

#include <string>

#include "ParameterBase.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

class StringParameter : public ParameterBase<OFX::StringParam, std::string> {
   public:
    struct Spec {
        std::string id_;
        std::optional<int> slot_;
        std::string label_;
        std::string shortLabel_;
        std::string longLabel_;
        std::string hint_;
        bool animates_ = true;
        std::string default_;
        bool visible_ = true;
        std::optional<VisibilityRule> visibility_;
        OFX::StringTypeEnum stringType_ = OFX::eStringTypeSingleLine;
    };

    explicit StringParameter(Spec spec);

    void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) override;

    [[nodiscard]] auto get(double time) const -> std::string;
    void set(double time, bool forceKeyFrame, const std::string& value);

   protected:
    void resetToDefault(double time) override;
    void setKeyFrameAtTimeImpl(double time) override;

   private:
    Spec spec_;
};

}  // namespace MugLab::OfxBase
