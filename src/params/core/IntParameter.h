#pragma once

#include <string>
#include <utility>

#include "ParameterBase.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

class IntParameter : public ParameterBase<OFX::IntParam, int> {
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
        std::optional<std::pair<int, int>> range_;
        std::optional<std::pair<int, int>> displayRange_;
    };

    explicit IntParameter(Spec spec);

    void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) override;

    [[nodiscard]] auto get(double time) const -> int;
    void set(double time, bool forceKeyFrame, int value);

   protected:
    void resetToDefault(double time) override;
    void setKeyFrameAtTimeImpl(double time) override;

   private:
    Spec spec_;
};

}  // namespace MugLab::OfxBase
