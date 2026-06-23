#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "interaction/CurrentState.h"
#include "interaction/HandlingRole.h"
#include "interaction/Intent.h"
#include "interaction/InteractionInput.h"
#include "interaction/SelectionItem.h"
#include "interaction/SnapshotState.h"
#include "params/ParameterManager.h"

namespace MugLab::OfxBase {

class OverlayRenderer;

enum class UseCaseLifecycle : std::uint8_t {
    Continue,
    Terminate
};

enum class ActivationTrigger : std::uint8_t {
    PenDown,
    PenMotion,
    Any
};

enum class TargetItemOperation : std::uint8_t {
    Add,
    Remove,
    RemoveIfSelf
};

struct UseCaseResult {
    std::optional<TargetItemOperation> targetOperation_;
    std::optional<std::vector<SelectionItem>> replacementSelection_;
    UseCaseLifecycle lifecycle_ = UseCaseLifecycle::Continue;
    IntentMap intents_;
};

class InteractionUseCase {
   public:
    virtual ~InteractionUseCase() = default;

    [[nodiscard]] virtual auto name() const -> std::string_view = 0;

    [[nodiscard]] virtual auto activationTrigger() const -> ActivationTrigger = 0;

    [[nodiscard]] virtual auto canHandle(const InteractionInput& input, const SnapshotState& current,
                                         const SnapshotState* activationSnapshot,
                                         const std::vector<std::string_view>& activeUseCases) const -> HandlingRole = 0;

    virtual void onStart(SnapshotState& snapshot, ParameterManager& parameterManager);

    virtual void onFinish(SnapshotState& snapshot, ParameterManager& parameterManager);

    virtual auto penDown(const InteractionInput& input, SnapshotState& snapshot, ParameterManager& parameterManager,
                         const std::vector<std::string_view>& activeUseCases) -> UseCaseResult = 0;

    virtual auto penMotion(const InteractionInput& input, SnapshotState& snapshot, const CurrentState& current,
                           ParameterManager& parameterManager,
                           const std::vector<std::string_view>& activeUseCases) -> UseCaseResult = 0;

    virtual auto penUp(const InteractionInput& input, SnapshotState& snapshot, ParameterManager& parameterManager,
                       const std::vector<std::string_view>& activeUseCases) -> UseCaseResult = 0;

    virtual auto onSelectionChanged(const std::vector<SelectionItem>& newSelection,
                                    const std::vector<std::string_view>& activeUseCases) -> UseCaseLifecycle;

    // Ported from mug-text-dissector where character/part selection is central.
    // In this SDK there is no such concept, so this mechanism is largely inactive.
    // Defaults to kAllIndices; override only if UseCase-level exclusivity is needed.
    [[nodiscard]] virtual auto targetSelectionItem() const -> SelectionItem {
        return SelectionItem{.characterIndex_ = kAllIndices, .partIndex_ = kAllIndices};
    }

    [[nodiscard]] virtual auto declaredEmittedIntents() const -> std::vector<std::string_view> {
        return {};
    }
    [[nodiscard]] virtual auto declaredConsumedIntents() const -> std::vector<std::string_view> {
        return {};
    }

    virtual auto onDraw(OverlayRenderer& overlayRenderer, const SnapshotState& snapshot, const CurrentState& current,
                        const std::vector<std::string_view>& activeUseCases) -> UseCaseResult;
};

}  // namespace MugLab::OfxBase
