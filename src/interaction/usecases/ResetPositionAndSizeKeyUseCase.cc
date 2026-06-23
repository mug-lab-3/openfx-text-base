#include "interaction/usecases/ResetPositionAndSizeKeyUseCase.h"

#include "interaction/InteractionInput.h"
#include "interaction/usecases/ResetPositionAndSizeCommand.h"
#include "ofxKeySyms.h"

namespace MugLab::OfxBase {

auto ResetPositionAndSizeKeyUseCase::name() const -> std::string_view {
    return "ResetPositionAndSizeKeyUseCase";
}

auto ResetPositionAndSizeKeyUseCase::canHandle(const InteractionInput& input,
                                               const SnapshotState& /*snapshot*/) const -> bool {
    const int key = input.keySymbol();
    const bool isN = (key == kOfxKey_n || key == kOfxKey_N);
    return input.modifiersMatch(Mod_Alt) && isN;
}

auto ResetPositionAndSizeKeyUseCase::onKeyDown(const InteractionInput& /*input*/, const SnapshotState& snapshot,
                                               ParameterManager& parameterManager) -> bool {
    ResetPositionAndSizeCommand command;
    command.execute(snapshot, parameterManager, *command.targetParameterIds().begin());
    return true;
}

}  // namespace MugLab::OfxBase
