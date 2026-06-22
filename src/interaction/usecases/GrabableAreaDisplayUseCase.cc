#include "interaction/usecases/GrabableAreaDisplayUseCase.h"
#include "overlay/OverlayRenderer.h"
#include <array>
#include <cmath>

namespace MugLab::OfxBase {

GrabableAreaDisplayUseCase::GrabableAreaDisplayUseCase() = default;

auto GrabableAreaDisplayUseCase::name() const -> std::string_view {
    return "GrabableAreaDisplayUseCase";
}

auto GrabableAreaDisplayUseCase::canHandle(const InteractionInput& input, const SnapshotState& current,
                                           const SnapshotState* activationSnapshot,
                                           const std::vector<std::string_view>& activeUseCases) const -> HandlingRole {
    const double mouseNormX = input.mousePos().x / input.canvasWidth();
    const double mouseNormY = 1.0 - (input.mousePos().y / input.canvasHeight());
    const double dx = mouseNormX - current.initialGlobalPos_.x_;
    const double dy = mouseNormY - current.initialGlobalPos_.y_;
    const bool inRange = (std::sqrt((dx * dx) + (dy * dy)) < 0.15);

    return inRange ? HandlingRole::Passive : HandlingRole::None;
}

auto GrabableAreaDisplayUseCase::onDraw(OverlayRenderer& renderer, const SnapshotState& snapshot, const CurrentState& current,
                                        const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    const ParamPoint2D currentPos = current.currentGlobalPos_;
    const double cx = currentPos.x_ * current.canvasWidth_;
    const double cy = currentPos.y_ * current.canvasHeight_;
    const double halfW = 0.15 * current.canvasWidth_;
    const double halfH = 0.15 * current.canvasHeight_;

    const std::array<OfxPointD, 4> rectPts = {{
        {.x = cx - halfW, .y = cy - halfH},
        {.x = cx + halfW, .y = cy - halfH},
        {.x = cx + halfW, .y = cy + halfH},
        {.x = cx - halfW, .y = cy + halfH}
    }};

    const OverlayRenderer::Style boxStyle = {.color_ = {.red_ = 1.0F, .green_ = 0.65F, .blue_ = 0.0F, .alpha_ = 0.7F}, .width_ = 1.5F};
    renderer.applyStyle(boxStyle);
    renderer.draw(PrimitiveType::LineLoop, rectPts.data(), static_cast<int>(rectPts.size()));

    return {.lifecycle_ = UseCaseLifecycle::Continue};
}


} // namespace MugLab::OfxBase
