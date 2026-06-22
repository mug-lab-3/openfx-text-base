#pragma once

#include "ofxsImageEffect.h"
#include "params/ParameterManager.h"

class OfxBasePlugin : public OFX::ImageEffect {
   public:
    explicit OfxBasePlugin(OfxImageEffectHandle handle);
    ~OfxBasePlugin() override;

    void render(const OFX::RenderArguments& args) override;
    void changedParam(const OFX::InstanceChangedArgs& args, const std::string& paramName) override;

    OFX::Clip* dstClip_ = nullptr;
    OFX::Clip* srcClip_ = nullptr;
    MugLab::OfxBase::ParameterManager params_;
};
