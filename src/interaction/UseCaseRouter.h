#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "interaction/Intent.h"
#include "interaction/SnapshotState.h"
#include "interaction/CurrentState.h"
#include "interaction/InteractionUseCase.h"

namespace MugLab::OfxBase {

class ParameterManager;
class PassiveUseCase;
class CommandUseCase;
class KeyUseCase;
class OverlayRenderer;

class UseCaseRouter {
   public:
    using UseCaseFactory = std::function<std::unique_ptr<InteractionUseCase>()>;
    using PassiveUseCaseFactory = std::function<std::unique_ptr<PassiveUseCase>()>;

    struct PotentialUseCase {
        std::unique_ptr<InteractionUseCase> useCase_;
        SnapshotState snapshot_;
        HandlingRole role_;
        bool isAlreadyActive_;
    };

    UseCaseRouter();
    ~UseCaseRouter();

    auto onPenDown(const InteractionInput& input, ParameterManager& parameterManager) -> bool;

    auto onPenMotion(const InteractionInput& input, const CurrentState& current,
                     ParameterManager& parameterManager) -> bool;

    auto onPenUp(const InteractionInput& input, ParameterManager& parameterManager) -> bool;
    void update(const InteractionInput& input, ParameterManager& parameterManager);

    auto onKeyDown(const InteractionInput& input, ParameterManager& parameterManager) -> bool;
    auto onKeyRepeat(const InteractionInput& input, ParameterManager& parameterManager) -> bool;
    auto onKeyUp(const InteractionInput& input, ParameterManager& parameterManager) -> bool;

    void onDraw(OverlayRenderer& overlayRenderer, const CurrentState& current, ParameterManager& parameterManager);

    [[nodiscard]] auto hasActiveUseCases() const -> bool;
    [[nodiscard]] auto getActiveUseCaseNames() const -> std::vector<std::string_view>;

    [[nodiscard]] auto getCurrentSelection() const -> std::vector<SelectionItem>;
    void updateSelection(const std::vector<SelectionItem>& selection, ParameterManager& parameterManager);
    void terminateAll(ParameterManager& parameterManager);
    void syncLayout(ParameterManager& parameterManager);
    void onTriggerCommand(std::string_view parameterId, double time, ParameterManager& parameterManager);

    // Whether any registered command use case is triggered by the given parameter id. Lets callers
    // gate dispatch without hardcoding the set of command button ids.
    [[nodiscard]] auto isCommandTrigger(std::string_view parameterId) const -> bool;

    [[nodiscard]] static auto snapshotWithIntents(const SnapshotState& snapshot, const InteractionUseCase& useCase,
                                                  const IntentRegistry& intents) -> SnapshotState;
    [[nodiscard]] static auto currentWithIntents(const CurrentState& current, const InteractionUseCase& useCase,
                                                 const IntentRegistry& intents) -> CurrentState;

   private:
    struct ActiveUseCase {
        std::unique_ptr<InteractionUseCase> useCase_;
        SnapshotState snapshot_;
    };

    static auto findWinnerPrimary(const std::vector<PotentialUseCase>& candidates) -> InteractionUseCase*;
    auto makeSnapshot(const InteractionInput& input, ParameterManager& parameterManager) const -> SnapshotState;

    auto dispatchPenDown(const InteractionInput& input, ParameterManager& parameterManager) -> bool;

    auto dispatchPenMotion(const InteractionInput& input, const CurrentState& current,
                           ParameterManager& parameterManager) -> bool;

    auto dispatchPenUp(const InteractionInput& input, ParameterManager& parameterManager) -> bool;

    using EventDispatcher = std::function<UseCaseResult(ActiveUseCase&, const std::vector<std::string_view>&)>;
    auto dispatchEvents(ParameterManager& parameterManager, const EventDispatcher& dispatcher) -> bool;

    void activateUseCases(const InteractionInput& input, ParameterManager& parameterManager, ActivationTrigger trigger);

    void handleSelectionChanged(ParameterManager& parameterManager);

    struct OwnedSelectionItem {
        SelectionItem item_;
        std::string ownerName_;
    };

    auto applySelectionResult(const UseCaseResult& result, ActiveUseCase& active,
                              ParameterManager& parameterManager) -> bool;
    void applyIntents(const std::string& useCaseName, const InteractionUseCase& useCase, const IntentMap& intents);
    void clearIntents(const std::string& useCaseName);
    void logIntents() const;
    void checkLayoutUpdate(ParameterManager& parameterManager);
    void clearCharacterFactories();
    void rebuildCharacterFactories();
    void cleanupInvalidSelection(ParameterManager& parameterManager);

    SelectionMode lastSelectionMode_ = SelectionMode::Global;
    std::vector<ActiveUseCase> currentUseCases_;
    std::vector<UseCaseFactory> factories_;
    std::vector<PassiveUseCaseFactory> characterPassiveFactories_;
    std::vector<UseCaseFactory> characterActiveFactories_;
    std::vector<PassiveUseCaseFactory> partPassiveFactories_;
    std::vector<UseCaseFactory> partActiveFactories_;
    std::vector<OwnedSelectionItem> currentSelection_;
    mutable std::recursive_mutex selectionMutex_;
    std::vector<std::unique_ptr<CommandUseCase>> commandUseCases_;
    // Aggregated parameter ids that trigger a command, collected once after the commands are created.
    std::unordered_set<std::string_view> commandTriggerIds_;
    std::vector<std::unique_ptr<KeyUseCase>> keyUseCases_;
    std::vector<KeyUseCase*> activeKeyUseCases_;
    IntentRegistry mergedIntents_;
};

} // namespace MugLab::OfxBase
