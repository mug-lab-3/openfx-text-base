#include "interaction/usecases/CursorHighlightUseCase.h"
#include "overlay/OverlayRenderer.h"
#include <array>

namespace MugLab::OfxBase {

CursorHighlightUseCase::CursorHighlightUseCase() = default;

auto CursorHighlightUseCase::name() const -> std::string_view {
    return "CursorHighlightUseCase";
}

auto CursorHighlightUseCase::canHandle(const InteractionInput& input, const SnapshotState& current,
                                       const SnapshotState* activationSnapshot,
                                       const std::vector<std::string_view>& activeUseCases) const -> HandlingRole {
    if (input.ctrl()) {
        return HandlingRole::Passive;
    }
    return HandlingRole::None;
}

auto CursorHighlightUseCase::onDraw(OverlayRenderer& renderer, const SnapshotState& snapshot, const CurrentState& current,
                                    const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    double mx = current.mouseCanvas_.x;
    double my = current.canvasHeight_ - current.mouseCanvas_.y;
    double r = 40.0;
    std::array<OfxPointD, 2> pts = {{
        {.x = mx - r, .y = my - r},
        {.x = mx + r, .y = my + r}
    }};
    OverlayRenderer::Color color = {.red_ = 1.0F, .green_ = 0.0F, .blue_ = 0.0F, .alpha_ = 1.0F};
    OverlayRenderer::Style style = {.color_ = color, .width_ = 2.0F};
    renderer.applyStyle(style);
    renderer.draw(PrimitiveType::Ellipse, pts.data(), 2);
    return {.lifecycle_ = UseCaseLifecycle::Continue};
}


} // namespace MugLab::OfxBase
