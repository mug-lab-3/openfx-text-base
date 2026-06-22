#include "InteractionInput.h"

#include <cmath>
#include "ofxKeySyms.h"
#include "ofxsInteract.h"

namespace MugLab::OfxBase {

namespace {
auto ofxToBlend2DCanvas(const BLPoint& penPosition, const BLSize& canvasSize) -> BLPoint {
    double x = penPosition.x;
    double y = penPosition.y;
    if (std::abs(x) > 1.0001 || std::abs(y) > 1.0001) {
        x /= canvasSize.w;
        y /= canvasSize.h;
    }
    return {x * canvasSize.w, (1.0 - y) * canvasSize.h};
}
}  // namespace

InteractionInput::InteractionInput()
    : canvasWidth_(0),
      canvasHeight_(0),
      mousePos_(0, 0),
      time_(0),
      renderScale_(1, 1),
      pixelScale_(1, 1),
      pressure_(0),
      modifiers_(Mod_None) {
}

InteractionInput::InteractionInput(const OFX::PenArgs& args, const BLPoint& convertedMousePos, Modifier modifiers)
    : canvasWidth_(0),
      canvasHeight_(0),
      mousePos_(convertedMousePos),
      time_(args.time),
      renderScale_(args.renderScale.x, args.renderScale.y),
      pixelScale_(args.pixelScale.x, args.pixelScale.y),
      pressure_(args.penPressure),
      modifiers_(modifiers) {
}

void InteractionInput::update(const OFX::DrawArgs& args, double canvasWidth, double canvasHeight) {
    canvasWidth_ = (canvasWidth > 0) ? canvasWidth : 0;
    canvasHeight_ = (canvasHeight > 0) ? canvasHeight : 0;
    time_ = args.time;
    renderScale_ = BLPoint(args.renderScale.x, args.renderScale.y);
}

void InteractionInput::update(const OFX::PenArgs& args) {
    BLSize canvasSize(canvasWidth_, canvasHeight_);
    mousePos_ = ofxToBlend2DCanvas({args.penPosition.x, args.penPosition.y}, canvasSize);
    time_ = args.time;
    renderScale_ = BLPoint(args.renderScale.x, args.renderScale.y);
    pixelScale_ = BLPoint(args.pixelScale.x, args.pixelScale.y);
    pressure_ = args.penPressure;
}

void InteractionInput::update(const OFX::KeyArgs& args, bool isDown) {
    keySymbol_ = isDown ? args.keySymbol : 0;
    if (args.keySymbol == kOfxKey_Control_L || args.keySymbol == kOfxKey_Control_R) {
        setModifier(Mod_Ctrl, isDown);
    } else if (args.keySymbol == kOfxKey_Alt_L || args.keySymbol == kOfxKey_Alt_R) {
        setModifier(Mod_Alt, isDown);
    } else if (args.keySymbol == kOfxKey_Shift_L || args.keySymbol == kOfxKey_Shift_R) {
        setModifier(Mod_Shift, isDown);
    }
}

void InteractionInput::setModifier(Modifier mod, bool isDown) {
    if (isDown) {
        modifiers_ = modifiers_ | mod;
    } else {
        modifiers_ = static_cast<Modifier>(static_cast<std::uint8_t>(modifiers_) & ~static_cast<std::uint8_t>(mod));
    }
}

auto InteractionInput::mousePos() const -> const BLPoint& {
    return mousePos_;
}

auto InteractionInput::time() const -> double {
    return time_;
}

auto InteractionInput::renderScale() const -> const BLPoint& {
    return renderScale_;
}

auto InteractionInput::pixelScale() const -> const BLPoint& {
    return pixelScale_;
}

auto InteractionInput::pressure() const -> double {
    return pressure_;
}

auto InteractionInput::canvasWidth() const -> double {
    return canvasWidth_;
}

auto InteractionInput::canvasHeight() const -> double {
    return canvasHeight_;
}

auto InteractionInput::canvasSize() const -> BLSize {
    return {canvasWidth_, canvasHeight_};
}

auto InteractionInput::modifiersMatch(Modifier mask) const -> bool {
    return modifiers_ == mask;
}

auto InteractionInput::hasModifier(Modifier mask) const -> bool {
    return (modifiers_ & mask) == mask;
}

auto InteractionInput::ctrl() const -> bool {
    return hasModifier(Mod_Ctrl);
}

auto InteractionInput::alt() const -> bool {
    return hasModifier(Mod_Alt);
}

auto InteractionInput::shift() const -> bool {
    return hasModifier(Mod_Shift);
}

auto InteractionInput::modifiers() const -> uint8_t {
    return static_cast<uint8_t>(modifiers_);
}

auto InteractionInput::keySymbol() const -> int {
    return keySymbol_;
}

}  // namespace MugLab::OfxBase
