#pragma once

#include <blend2d.h>
#include <ofxCore.h>

#include <vector>

#include "interaction/Intent.h"
#include "interaction/SelectionItem.h"
#include "params/core/Types.h"

namespace MugLab::OfxBase {

struct CurrentState {
    double time_ = 0.0;
    SelectionMode selectionMode_ = SelectionMode::Global;
    std::vector<SelectionItem> selection_;
    IntentRegistry intents_;

    // Added for CirclePassiveUseCase
    BLPoint mouseCanvas_ = {0.0, 0.0};
    uint8_t modifiers_ = 0;
    double canvasWidth_ = 0.0;
    double canvasHeight_ = 0.0;

    // Added for hover feedback
    ParamPoint2D currentGlobalPos_ = {0.0, 0.0};
};

}  // namespace MugLab::OfxBase
