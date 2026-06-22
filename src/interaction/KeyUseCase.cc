#include "interaction/KeyUseCase.h"

namespace MugLab::OfxBase {

auto KeyUseCase::onKeyDown(const InteractionInput& /*input*/, const SnapshotState& /*snapshot*/,
                           ParameterManager& /*parameterManager*/) -> bool {
    return false;
}

auto KeyUseCase::onKeyRepeat(const InteractionInput& /*input*/, const SnapshotState& /*snapshot*/,
                             ParameterManager& /*parameterManager*/) -> bool {
    return false;
}

auto KeyUseCase::onKeyUp(const InteractionInput& /*input*/, const SnapshotState& /*snapshot*/,
                         ParameterManager& /*parameterManager*/) -> bool {
    return false;
}

void KeyUseCase::onDraw(OverlayRenderer& /*renderer*/, const CurrentState& /*current*/) {
}

}  // namespace MugLab::OfxBase
