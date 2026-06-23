#include "interaction/Interact.h"

#include <mutex>

#include "debugger/LogManager.h"
#include "interaction/StateFactory.h"
#include "ofxDrawSuite.h"
#include "overlay/OfxDrawSuiteRenderer.h"
#include "params/ParameterManager.h"

namespace MugLab::OfxBase {

Interact::Interact(OfxInteractHandle handle, OFX::ImageEffect* effect, ParameterManager& parameterManager)
    : OFX::OverlayInteract(handle), parameterManager_(parameterManager) {
    _effect = effect;
}

auto Interact::draw(const OFX::DrawArgs& args) -> bool {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    needsRedraw_ = false;
    double projectWidth = 1920.0;
    double projectHeight = 1080.0;
    const OfxPointD projectSize = _effect->getProjectSize();
    if (projectSize.x > 0.0 && projectSize.y > 0.0) {
        projectWidth = projectSize.x;
        projectHeight = projectSize.y;
    } else {
        auto* outputClip = _effect->fetchClip(kOfxImageEffectOutputClipName);
        if (outputClip != nullptr && outputClip->isConnected()) {
            const auto regionOfDefinition = outputClip->getRegionOfDefinition(args.time);
            projectWidth = regionOfDefinition.x2 - regionOfDefinition.x1;
            projectHeight = regionOfDefinition.y2 - regionOfDefinition.y1;
        }
    }
    input_.update(args, projectWidth, projectHeight);
    useCaseRouter_.update(input_, parameterManager_);

    static auto* drawSuite = static_cast<OfxDrawSuiteV1*>(const_cast<void*>(OFX::fetchSuite(kOfxDrawSuite, 1, true)));

    if (drawSuite == nullptr || currentContext_ == nullptr) {
        return false;
    }

    const CanvasSize canvas = {.width_ = projectWidth, .height_ = projectHeight};
    OfxDrawSuiteRenderer renderer(canvas, drawSuite, currentContext_);

    CurrentState current = StateFactory::createCurrent(args.time, projectWidth, projectHeight, {1.0, 1.0},
                                                       useCaseRouter_, input_, parameterManager_);
    useCaseRouter_.onDraw(renderer, current, parameterManager_);
    return true;
}

auto Interact::penDown(const OFX::PenArgs& args) -> bool {
    LOG_INFO("EVENT_PEN_DOWN", "x", args.penPosition.x, "y", args.penPosition.y, "pressure", args.penPressure);
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    input_.update(args);
    const bool handled = useCaseRouter_.onPenDown(input_, parameterManager_);
    triggerRedraw();
    return handled;
}

auto Interact::penMotion(const OFX::PenArgs& args) -> bool {
    LOG_INFO("EVENT_PEN_MOTION", "x", args.penPosition.x, "y", args.penPosition.y, "pressure", args.penPressure);
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    input_.update(args);
    CurrentState current = StateFactory::createCurrent(args.time, input_.canvasWidth(), input_.canvasHeight(),
                                                       {1.0, 1.0}, useCaseRouter_, input_, parameterManager_);
    const bool handled = useCaseRouter_.onPenMotion(input_, current, parameterManager_);
    triggerRedraw();
    return handled;
}

auto Interact::penUp(const OFX::PenArgs& args) -> bool {
    LOG_INFO("EVENT_PEN_UP", "x", args.penPosition.x, "y", args.penPosition.y);
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    input_.update(args);
    const bool handled = useCaseRouter_.onPenUp(input_, parameterManager_);
    triggerRedraw();
    return handled;
}

auto Interact::keyDown(const OFX::KeyArgs& args) -> bool {
    LOG_INFO("EVENT_KEY_DOWN", "sym", args.keyString);
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    input_.update(args, true);
    const bool handled = useCaseRouter_.onKeyDown(input_, parameterManager_);
    triggerRedraw();
    return handled;
}

auto Interact::keyUp(const OFX::KeyArgs& args) -> bool {
    LOG_INFO("EVENT_KEY_UP", "sym", args.keyString);
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    input_.update(args, false);
    const bool handled = useCaseRouter_.onKeyUp(input_, parameterManager_);
    triggerRedraw();
    return handled;
}

auto Interact::keyRepeat(const OFX::KeyArgs& args) -> bool {
    LOG_INFO("EVENT_KEY_REPEAT", "sym", args.keyString);
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    input_.update(args, true);
    const bool handled = useCaseRouter_.onKeyRepeat(input_, parameterManager_);
    if (handled) {
        triggerRedraw();
    }
    return handled;
}

void Interact::triggerRedraw() {
    if (!needsRedraw_) {
        requestRedraw();
        needsRedraw_ = true;
    }
}

}  // namespace MugLab::OfxBase
