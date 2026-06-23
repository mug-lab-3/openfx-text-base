#include "interaction/usecases/DragUseCase.h"

#include "params/ParamIds.h"
#include "params/ParameterManager.h"

namespace MugLab::OfxBase {

DragUseCase::DragUseCase() = default;

auto DragUseCase::name() const -> std::string_view {
    return "DragUseCase";
}

auto DragUseCase::canHandle(const InteractionInput& input, const SnapshotState& current,
                            const SnapshotState* activationSnapshot,
                            const std::vector<std::string_view>& activeUseCases) const -> HandlingRole {
    if (activationSnapshot != nullptr) {
        return HandlingRole::Primary;
    }
    return current.intents_.has("isInGrabArea") ? HandlingRole::Primary : HandlingRole::None;
}

void DragUseCase::onStart(SnapshotState& snapshot, ParameterManager& parameterManager) {
    parameterManager.beginEditBlock("Drag");
}

void DragUseCase::onFinish(SnapshotState& snapshot, ParameterManager& parameterManager) {
    parameterManager.endEditBlock();
}

auto DragUseCase::penDown(const InteractionInput& input, SnapshotState& snapshot, ParameterManager& parameterManager,
                          const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    UseCaseResult result;
    result.targetOperation_ = TargetItemOperation::Add;
    result.lifecycle_ = UseCaseLifecycle::Continue;
    result.intents_["isDragging"] = IntentValues{true};
    return result;
}

auto DragUseCase::penMotion(const InteractionInput& input, SnapshotState& snapshot, const CurrentState& current,
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
    const double deltaY = -deltaCanvas.y / canvasSize.h;  // Flip Y-up

    const ParamPoint2D newPos = {snapshot.initialGlobalPos_.x_ + deltaX, snapshot.initialGlobalPos_.y_ + deltaY};

    parameterManager.setDouble2D(ParamIds::kPosition, newPos, time);

    UseCaseResult result;
    result.lifecycle_ = UseCaseLifecycle::Continue;
    result.intents_["isDragging"] = IntentValues{true};
    return result;
}

auto DragUseCase::penUp(const InteractionInput& input, SnapshotState& snapshot, ParameterManager& parameterManager,
                        const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    UseCaseResult result;
    result.lifecycle_ = UseCaseLifecycle::Terminate;
    result.intents_["isDragging"] = std::nullopt;
    return result;
}

auto DragUseCase::declaredEmittedIntents() const -> std::vector<std::string_view> {
    return {"isDragging"};
}

auto DragUseCase::declaredConsumedIntents() const -> std::vector<std::string_view> {
    return {"isInGrabArea"};
}

}  // namespace MugLab::OfxBase
