#include "interaction/usecases/GrabableAreaDisplayUseCase.h"

#include <array>
#include <cmath>

#include "overlay/OverlayRenderer.h"

namespace MugLab::OfxBase {

GrabableAreaDisplayUseCase::GrabableAreaDisplayUseCase() = default;

auto GrabableAreaDisplayUseCase::name() const -> std::string_view {
    return "GrabableAreaDisplayUseCase";
}

auto GrabableAreaDisplayUseCase::isMouseInGrabArea(const InteractionInput& input, double textNormX,
                                                   double textNormY) -> bool {
    const double mouseNormX = input.mousePos().x / input.canvasWidth();
    const double mouseNormY = 1.0 - (input.mousePos().y / input.canvasHeight());
    const double dx = mouseNormX - textNormX;
    const double dy = mouseNormY - textNormY;
    return (std::abs(dx) < kGrabAreaHalfExtent && std::abs(dy) < kGrabAreaHalfExtent);
}

auto GrabableAreaDisplayUseCase::canHandle(const InteractionInput& input, const SnapshotState& current,
                                           const SnapshotState* activationSnapshot,
                                           const std::vector<std::string_view>& activeUseCases) const -> HandlingRole {
    const bool inRange = isMouseInGrabArea(input, current.initialGlobalPos_.x_, current.initialGlobalPos_.y_);
    return inRange ? HandlingRole::Passive : HandlingRole::None;
}

auto GrabableAreaDisplayUseCase::onPassivePenMotion(
    const InteractionInput& input, SnapshotState& snapshot, const CurrentState& current,
    const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    const bool inRange = isMouseInGrabArea(input, current.currentGlobalPos_.x_, current.currentGlobalPos_.y_);

    UseCaseResult result;
    result.lifecycle_ = UseCaseLifecycle::Continue;
    result.intents_["isInGrabArea"] = inRange ? std::optional<IntentValues>{IntentValues{true}} : std::nullopt;
    return result;
}

auto GrabableAreaDisplayUseCase::onDraw(OverlayRenderer& renderer, const SnapshotState& snapshot,
                                        const CurrentState& current,
                                        const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    const ParamPoint2D currentPos = current.currentGlobalPos_;
    const double cx = currentPos.x_ * current.canvasWidth_;
    const double cy = currentPos.y_ * current.canvasHeight_;
    const double halfW = kGrabAreaHalfExtent * current.canvasWidth_;
    const double halfH = kGrabAreaHalfExtent * current.canvasHeight_;

    const std::array<OfxPointD, 4> rectPts = {{{.x = cx - halfW, .y = cy - halfH},
                                               {.x = cx + halfW, .y = cy - halfH},
                                               {.x = cx + halfW, .y = cy + halfH},
                                               {.x = cx - halfW, .y = cy + halfH}}};

    const OverlayRenderer::Style boxStyle = {.color_ = {.red_ = 1.0F, .green_ = 0.65F, .blue_ = 0.0F, .alpha_ = 0.7F},
                                             .width_ = 1.5F};
    renderer.applyStyle(boxStyle);
    renderer.draw(PrimitiveType::LineLoop, rectPts.data(), static_cast<int>(rectPts.size()));

    // Intent emission is handled in onPassivePenMotion; onDraw only renders.
    return {.lifecycle_ = UseCaseLifecycle::Continue};
}

}  // namespace MugLab::OfxBase
