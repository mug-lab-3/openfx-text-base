#pragma once

#include "interaction/PassiveUseCase.h"

namespace MugLab::OfxBase {

class DragFeedbackUseCase : public PassiveUseCase {
   public:
    DragFeedbackUseCase();
    virtual ~DragFeedbackUseCase() = default;

    [[nodiscard]] auto name() const -> std::string_view override;
    [[nodiscard]] auto activationTrigger() const -> ActivationTrigger override {
        return ActivationTrigger::Any;
    }

    [[nodiscard]] auto canHandle(const InteractionInput& input, const SnapshotState& current,
                                 const SnapshotState* activationSnapshot,
                                 const std::vector<std::string_view>& activeUseCases) const -> HandlingRole override;

    auto onDraw(OverlayRenderer& overlayRenderer, const SnapshotState& snapshot, const CurrentState& current,
                const std::vector<std::string_view>& activeUseCases) -> UseCaseResult override;

    [[nodiscard]] auto declaredConsumedIntents() const -> std::vector<std::string_view> override {
        return {"isDragging"};
    }
};

}  // namespace MugLab::OfxBase
