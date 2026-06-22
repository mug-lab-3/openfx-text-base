#pragma once

#include <ofxCore.h>
#include <blend2d.h>
#include <vector>
#include "interaction/CurrentState.h"
#include "interaction/SnapshotState.h"
#include "interaction/InteractionInput.h"

namespace MugLab::OfxBase {

class ParameterManager;
class UseCaseRouter;

class StateFactory {
   public:
    static auto createCurrent(double time, double canvasWidth, double canvasHeight,
                              OfxPointD renderScale, const UseCaseRouter& useCaseRouter,
                              const InteractionInput& input,
                              ParameterManager& parameterManager) -> CurrentState;

    static auto createSnapshot(double time, double canvasWidth, double canvasHeight,
                               OfxPointD renderScale, const BLPoint& mouseCanvas, const OfxPointD& mouseOfx,
                               SelectionMode selectionMode, const std::vector<SelectionItem>& selection,
                               uint8_t modifiers, ParameterManager& parameterManager) -> SnapshotState;
};

} // namespace MugLab::OfxBase
