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

   protected:
    auto onPassivePenMotion(const InteractionInput& input, SnapshotState& snapshot, const CurrentState& current,
                            const std::vector<std::string_view>& activeUseCases) -> UseCaseResult override;

   private:
    // Half-extent of the grabbable area in normalized canvas coordinates.
    static constexpr double kGrabAreaHalfExtent = 0.15;

    // True when the mouse sits within the grabbable rectangle around the given normalized text position.
    [[nodiscard]] static auto isMouseInGrabArea(const InteractionInput& input, double textNormX,
                                                double textNormY) -> bool;
};

}  // namespace MugLab::OfxBase
