#pragma once

#include "interaction/InteractionUseCase.h"

namespace MugLab::OfxBase {

class DragUseCase : public InteractionUseCase {
   public:
    DragUseCase();
    virtual ~DragUseCase() = default;

    [[nodiscard]] auto name() const -> std::string_view override;
    [[nodiscard]] auto activationTrigger() const -> ActivationTrigger override {
        return ActivationTrigger::PenDown;
    }

    [[nodiscard]] auto canHandle(const InteractionInput& input, const SnapshotState& current,
                                 const SnapshotState* activationSnapshot,
                                 const std::vector<std::string_view>& activeUseCases) const -> HandlingRole override;

    void onStart(SnapshotState& snapshot, ParameterManager& parameterManager) override;
    void onFinish(SnapshotState& snapshot, ParameterManager& parameterManager) override;

    auto penDown(const InteractionInput& input, SnapshotState& snapshot, ParameterManager& parameterManager,
                 const std::vector<std::string_view>& activeUseCases) -> UseCaseResult override;

    auto penMotion(const InteractionInput& input, SnapshotState& snapshot, const CurrentState& current,
                   ParameterManager& parameterManager,
                   const std::vector<std::string_view>& activeUseCases) -> UseCaseResult override;

    auto penUp(const InteractionInput& input, SnapshotState& snapshot, ParameterManager& parameterManager,
               const std::vector<std::string_view>& activeUseCases) -> UseCaseResult override;

    [[nodiscard]] auto declaredEmittedIntents() const -> std::vector<std::string_view> override;
};

} // namespace MugLab::OfxBase
