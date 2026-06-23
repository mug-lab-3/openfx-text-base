#include "interaction/usecases/DragFeedbackUseCase.h"

#include <array>

#include "overlay/OverlayRenderer.h"

namespace MugLab::OfxBase {

DragFeedbackUseCase::DragFeedbackUseCase() = default;

auto DragFeedbackUseCase::name() const -> std::string_view {
    return "DragFeedbackUseCase";
}

auto DragFeedbackUseCase::canHandle(const InteractionInput& input, const SnapshotState& current,
                                    const SnapshotState* activationSnapshot,
                                    const std::vector<std::string_view>& activeUseCases) const -> HandlingRole {
    return current.intents_.has("isDragging") ? HandlingRole::Passive : HandlingRole::None;
}

auto DragFeedbackUseCase::onDraw(OverlayRenderer& overlayRenderer, const SnapshotState& snapshot,
                                 const CurrentState& current,
                                 const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
    const double cx = current.currentGlobalPos_.x_ * current.canvasWidth_;
    const double cy = current.currentGlobalPos_.y_ * current.canvasHeight_;
    constexpr double arm = 40.0;

    const std::array<OfxPointD, 2> hLine = {{
        {.x = cx - arm, .y = cy},
        {.x = cx + arm, .y = cy},
    }};
    const std::array<OfxPointD, 2> vLine = {{
        {.x = cx, .y = cy - arm},
        {.x = cx, .y = cy + arm},
    }};

    constexpr OverlayRenderer::Style style = {
        .color_ = {.red_ = 1.0F, .green_ = 1.0F, .blue_ = 1.0F, .alpha_ = 0.9F},
        .width_ = 2.0F,
    };
    overlayRenderer.applyStyle(style);
    overlayRenderer.draw(PrimitiveType::Lines, hLine.data(), 2);
    overlayRenderer.draw(PrimitiveType::Lines, vLine.data(), 2);

    return {.lifecycle_ = UseCaseLifecycle::Continue};
}

}  // namespace MugLab::OfxBase
