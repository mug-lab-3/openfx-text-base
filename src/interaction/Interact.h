#pragma once

#include <ofxCore.h>

#include <mutex>

#include "interaction/InteractionInput.h"
#include "interaction/UseCaseRouter.h"
#include "ofxDrawSuite.h"
#include "ofxsInteract.h"

namespace MugLab::OfxBase {

class ParameterManager;

class Interact : public OFX::OverlayInteract {
   public:
    Interact(OfxInteractHandle handle, OFX::ImageEffect* effect, ParameterManager& parameterManager);
    ~Interact() override = default;

    auto draw(const OFX::DrawArgs& args) -> bool override;
    auto penDown(const OFX::PenArgs& args) -> bool override;
    auto penMotion(const OFX::PenArgs& args) -> bool override;
    auto penUp(const OFX::PenArgs& args) -> bool override;
    auto keyDown(const OFX::KeyArgs& args) -> bool override;
    auto keyUp(const OFX::KeyArgs& args) -> bool override;
    auto keyRepeat(const OFX::KeyArgs& args) -> bool override;

    OfxDrawContextHandle currentContext_ = nullptr;

    void triggerRedraw();

   private:
    MugLab::OfxBase::ParameterManager& parameterManager_;
    MugLab::OfxBase::InteractionInput input_;
    MugLab::OfxBase::UseCaseRouter useCaseRouter_;
    std::recursive_mutex mutex_;
    bool needsRedraw_ = false;
};

}  // namespace MugLab::OfxBase
