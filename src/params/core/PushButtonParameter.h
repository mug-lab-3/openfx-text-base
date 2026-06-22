#pragma once

#include <optional>
#include <string>

#include "ParameterNode.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

class PushButtonParameter : public ParameterNode {
   public:
    struct Spec {
        std::string id_;
        std::optional<int> slot_;
        std::string label_;
        std::string shortLabel_;
        std::string longLabel_;
        std::string hint_;
        bool visible_ = true;
        std::optional<VisibilityRule> visibility_;
    };

    explicit PushButtonParameter(Spec spec);

    void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) override;
    void fetch(OFX::ImageEffect& effectInstance) override;
    void reset(bool /*deleteAnimation*/, double /*time*/) override {
    }
    void deleteKeysInRange(double /*lo*/, double /*hi*/) override {
    }
    void setKeyFrameAtTime(double /*time*/) override {
    }

    [[nodiscard]] auto raw() const -> OFX::PushButtonParam* {
        return raw_;
    }

   protected:
    void applyVisible(bool visible) override;

   private:
    Spec spec_;
    OFX::PushButtonParam* raw_ = nullptr;
};

}  // namespace MugLab::OfxBase
