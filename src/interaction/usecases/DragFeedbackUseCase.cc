#include "interaction/usecases/DragFeedbackUseCase.h"

#include "params/ParamIds.h"
#include "params/ParameterManager.h"

namespace MugLab::OfxBase {

DragFeedbackUseCase::DragFeedbackUseCase() = default;

auto DragFeedbackUseCase::name() const -> std::string_view {
    return "DragFeedbackUseCase";
}

auto DragFeedbackUseCase::canHandle(const InteractionInput& input, const SnapshotState& current,
                                    const SnapshotState* activationSnapshot,
                                    const std::vector<std::string_view>& activeUseCases) const -> HandlingRole {
    return current.intents_.has("isDragging") ? HandlingRole::Passive : HandlingRole::None;
}

void DragFeedbackUseCase::onStart(SnapshotState& snapshot, ParameterManager& parameterManager) {
    originalFontSize_ = parameterManager.getDouble(ParamIds::kFontSize, snapshot.time_);
    parameterManager.setDouble(ParamIds::kFontSize, originalFontSize_ * 1.3, snapshot.time_);
}

void DragFeedbackUseCase::onFinish(SnapshotState& snapshot, ParameterManager& parameterManager) {
    parameterManager.setDouble(ParamIds::kFontSize, originalFontSize_, snapshot.time_);
}


}  // namespace MugLab::OfxBase
