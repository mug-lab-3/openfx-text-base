#include "interaction/StateFactory.h"
#include "interaction/UseCaseRouter.h"
#include "params/ParamIds.h"
#include "params/ParameterManager.h"

namespace MugLab::OfxBase {

auto StateFactory::createCurrent(double time, double canvasWidth, double canvasHeight,
                                 OfxPointD renderScale, const UseCaseRouter& useCaseRouter,
                                 const InteractionInput& input,
                                 ParameterManager& parameterManager) -> CurrentState {
    CurrentState state;
    state.time_ = time;
    state.selectionMode_ = parameterManager.getSelectionMode(time);
    state.selection_ = useCaseRouter.getCurrentSelection();
    state.mouseCanvas_ = input.mousePos();
    state.modifiers_ = input.modifiers();
    state.canvasWidth_ = canvasWidth;
    state.canvasHeight_ = canvasHeight;
    state.currentGlobalPos_ = parameterManager.getDouble2D(ParamIds::kPosition, time);
    return state;
}

auto StateFactory::createSnapshot(double time, double canvasWidth, double canvasHeight,
                                  OfxPointD renderScale, const BLPoint& mouseCanvas, const OfxPointD& mouseOfx,
                                  SelectionMode selectionMode, const std::vector<SelectionItem>& selection,
                                  uint8_t modifiers, ParameterManager& parameterManager) -> SnapshotState {
    SnapshotState state;
    state.canvasWidth_ = canvasWidth;
    state.canvasHeight_ = canvasHeight;
    state.time_ = time;
    state.renderScale_ = renderScale;
    state.selectionMode_ = selectionMode;
    state.selection_ = selection;
    state.initialModifiers_ = modifiers;
    state.initialMouseCanvas_ = mouseCanvas;
    state.initialMouseOfx_ = mouseOfx;

    // Set initial baseline properties for sample use cases
    state.initialGlobalPos_ = parameterManager.getDouble2D(ParamIds::kPosition, time);
    state.initialFontSize_ = parameterManager.getDouble(ParamIds::kFontSize, time);

    return state;
}

} // namespace MugLab::OfxBase
