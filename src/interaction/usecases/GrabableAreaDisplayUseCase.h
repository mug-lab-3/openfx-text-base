#pragma once

#include "interaction/PassiveUseCase.h"

namespace MugLab::OfxBase {

class GrabableAreaDisplayUseCase : public PassiveUseCase {
   public:
    GrabableAreaDisplayUseCase();
    virtual ~GrabableAreaDisplayUseCase() = default;

    [[nodiscard]] auto name() const -> std::string_view override;
    [[nodiscard]] auto activationTrigger() const -> ActivationTrigger override {
        return ActivationTrigger::Any;
    }

    [[nodiscard]] auto canHandle(const InteractionInput& input, const SnapshotState& current,
                                 const SnapshotState* activationSnapshot,
                                 const std::vector<std::string_view>& activeUseCases) const -> HandlingRole override;

    auto onDraw(OverlayRenderer& renderer, const SnapshotState& snapshot, const CurrentState& current,
                const std::vector<std::string_view>& activeUseCases) -> UseCaseResult override;

    [[nodiscard]] auto declaredEmittedIntents() const -> std::vector<std::string_view> override {
        return {"isInGrabArea"};
    }
};

}  // namespace MugLab::OfxBase
