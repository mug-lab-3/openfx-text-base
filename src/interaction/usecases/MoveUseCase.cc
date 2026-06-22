#include "interaction/usecases/MoveUseCase.h"
#include "params/ParamIds.h"
#include "params/ParameterManager.h"
#include <cmath>

namespace MugLab::OfxBase {

MoveUseCase::MoveUseCase() = default;

auto MoveUseCase::name() const -> std::string_view {
    return "MoveUseCase";
}

auto MoveUseCase::canHandle(const InteractionInput& input, const SnapshotState& current,
                            const SnapshotState* activationSnapshot,
                            const std::vector<std::string_view>& activeUseCases) const -> HandlingRole {
    HandlingRole role = HandlingRole::None;
    if (activationSnapshot != nullptr) {
        role = HandlingRole::Primary;
    } else {
        if (current.selectionMode_ == SelectionMode::Global) {
            ParamPoint2D currentPos = current.initialGlobalPos_;
            // Normalize mouse position
            double mouseNormX = current.initialMouseCanvas_.x / current.canvasWidth_;
            double mouseNormY = 1.0 - (current.initialMouseCanvas_.y / current.canvasHeight_);
            double dx = mouseNormX - currentPos.x_;
            double dy = mouseNormY - currentPos.y_;
            if (std::sqrt(dx * dx + dy * dy) < 0.15) {
                role = HandlingRole::Primary;
            }
        }
    }
    return role;
}

void MoveUseCase::onStart(SnapshotState& snapshot, ParameterManager& parameterManager) {
    parameterManager.beginEditBlock("Move");
}

void MoveUseCase::onFinish(SnapshotState& snapshot, ParameterManager& parameterManager) {
    parameterManager.endEditBlock();
}

auto MoveUseCase::penDown(const InteractionInput& input, SnapshotState& snapshot,
                          ParameterManager& parameterManager,
                          const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    UseCaseResult result;
    result.targetOperation_ = TargetItemOperation::Add;
    result.lifecycle_ = UseCaseLifecycle::Continue;
    result.intents_["isDragging"] = IntentValues{ true };
    return result;
}

auto MoveUseCase::penMotion(const InteractionInput& input, SnapshotState& snapshot, const CurrentState& current,
                            ParameterManager& parameterManager,
                            const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    if (current.selectionMode_ != SelectionMode::Global) {
        return {.lifecycle_ = UseCaseLifecycle::Terminate};
    }

    const BLSize canvasSize = input.canvasSize();
    const BLPoint currentMousePos = input.mousePos();
    const double time = input.time();

    const BLPoint deltaCanvas = currentMousePos - snapshot.initialMouseCanvas_;
    const double deltaX = deltaCanvas.x / canvasSize.w;
    const double deltaY = -deltaCanvas.y / canvasSize.h; // Flip Y-up

    const ParamPoint2D newPos = {snapshot.initialGlobalPos_.x_ + deltaX,
                                 snapshot.initialGlobalPos_.y_ + deltaY};

    parameterManager.setDouble2D(ParamIds::kPosition, newPos, time);

    UseCaseResult result;
    result.lifecycle_ = UseCaseLifecycle::Continue;
    result.intents_["isDragging"] = IntentValues{ true };
    return result;
}

auto MoveUseCase::penUp(const InteractionInput& input, SnapshotState& snapshot,
                        ParameterManager& parameterManager,
                        const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    UseCaseResult result;
    result.lifecycle_ = UseCaseLifecycle::Terminate;
    result.intents_["isDragging"] = std::nullopt;
    return result;
}

auto MoveUseCase::declaredEmittedIntents() const -> std::vector<std::string_view> {
    return { "isDragging" };
}

} // namespace MugLab::OfxBase
