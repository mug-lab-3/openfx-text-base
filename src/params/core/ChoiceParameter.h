#pragma once

#include <string>
#include <vector>

#include "ParameterBase.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

class ChoiceParameter : public ParameterBase<OFX::ChoiceParam, int> {
   public:
    struct Spec {
        std::string id_;
        std::optional<int> slot_;
        std::string label_;
        std::string shortLabel_;
        std::string longLabel_;
        std::string hint_;
        bool animates_ = true;
        int default_ = 0;
        bool visible_ = true;
        std::optional<VisibilityRule> visibility_;
        std::vector<std::string> options_;
    };

    explicit ChoiceParameter(Spec spec);

    void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) override;

    [[nodiscard]] auto get(double time) const -> int;
    void set(double time, bool forceKeyFrame, int value);
    void set(double time, bool forceKeyFrame, const std::string& optionName);

    [[nodiscard]] auto options() const -> const std::vector<std::string>& {
        return spec_.options_;
    }

   protected:
    void resetToDefault(double time) override;
    void setKeyFrameAtTimeImpl(double time) override;

   private:
    Spec spec_;
};

}  // namespace MugLab::OfxBase
