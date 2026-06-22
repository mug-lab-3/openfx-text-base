#include "interaction/InteractionUseCase.h"

namespace MugLab::OfxBase {

void InteractionUseCase::onStart([[maybe_unused]] SnapshotState& snapshot,
                                 [[maybe_unused]] ParameterManager& parameterManager) {
}

void InteractionUseCase::onFinish([[maybe_unused]] SnapshotState& snapshot,
                                  [[maybe_unused]] ParameterManager& parameterManager) {
}

auto InteractionUseCase::onSelectionChanged([[maybe_unused]] const std::vector<SelectionItem>& newSelection,
                                            [[maybe_unused]] const std::vector<std::string_view>& activeUseCases)
    -> UseCaseLifecycle {
    return UseCaseLifecycle::Continue;
}

auto InteractionUseCase::onDraw([[maybe_unused]] OverlayRenderer& overlayRenderer,
                                [[maybe_unused]] const SnapshotState& snapshot,
                                [[maybe_unused]] const CurrentState& current,
                                [[maybe_unused]] const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    return {.lifecycle_ = UseCaseLifecycle::Continue};
}

}  // namespace MugLab::OfxBase
