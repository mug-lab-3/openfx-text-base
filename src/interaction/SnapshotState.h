#pragma once

#include <blend2d.h>
#include <ofxCore.h>

#include <vector>

#include "interaction/HandlingRole.h"
#include "interaction/Intent.h"
#include "interaction/SelectionItem.h"
#include "params/core/Types.h"

namespace MugLab::OfxBase {

struct SnapshotState {
    double canvasWidth_ = 0.0;
    double canvasHeight_ = 0.0;
    double time_ = 0.0;
    OfxPointD renderScale_ = {1.0, 1.0};

    HandlingRole handlingRole_ = HandlingRole::None;

    SelectionMode selectionMode_ = SelectionMode::Global;
    std::vector<SelectionItem> selection_;
    uint8_t initialModifiers_ = 0;

    OfxPointD initialMouseOfx_ = {0.0, 0.0};
    BLPoint initialMouseCanvas_ = {0.0, 0.0};

    std::vector<SelectionItem> hoveredItems_;

    IntentRegistry intents_;

    // Added for MoveUseCase
    ParamPoint2D initialGlobalPos_ = {0.0, 0.0};
    double initialFontSize_ = 0.0;
};

}  // namespace MugLab::OfxBase
