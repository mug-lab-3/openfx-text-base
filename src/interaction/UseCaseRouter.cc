#include "interaction/UseCaseRouter.h"

#include <algorithm>
#include <functional>
#include <typeindex>

#include "debugger/LogManager.h"
#include "interaction/CommandUseCase.h"
#include "interaction/KeyUseCase.h"
#include "interaction/PassiveUseCase.h"
#include "interaction/StateFactory.h"
#include "interaction/usecases/CursorHighlightUseCase.h"
#include "interaction/usecases/DragFeedbackUseCase.h"
#include "interaction/usecases/DragUseCase.h"
#include "interaction/usecases/GrabableAreaDisplayUseCase.h"
#include "interaction/usecases/ResetPositionAndSizeCommand.h"
#include "interaction/usecases/ResetPositionAndSizeKeyUseCase.h"
#include "overlay/OverlayRenderer.h"
#include "params/ParameterManager.h"

namespace MugLab::OfxBase {

UseCaseRouter::UseCaseRouter() {
    factories_.emplace_back(
        [this]() -> std::unique_ptr<InteractionUseCase> { return std::make_unique<DragUseCase>(); });
    factories_.emplace_back(
        [this]() -> std::unique_ptr<InteractionUseCase> { return std::make_unique<CursorHighlightUseCase>(); });
    factories_.emplace_back(
        [this]() -> std::unique_ptr<InteractionUseCase> { return std::make_unique<GrabableAreaDisplayUseCase>(); });
    factories_.emplace_back(
        [this]() -> std::unique_ptr<InteractionUseCase> { return std::make_unique<DragFeedbackUseCase>(); });

    commandUseCases_.emplace_back(std::make_unique<ResetPositionAndSizeCommand>());

    for (const auto& command : commandUseCases_) {
        const auto ids = command->targetParameterIds();
        commandTriggerIds_.insert(ids.begin(), ids.end());
    }

    keyUseCases_.emplace_back(std::make_unique<ResetPositionAndSizeKeyUseCase>());
}

UseCaseRouter::~UseCaseRouter() = default;

auto UseCaseRouter::onPenDown(const InteractionInput& input, ParameterManager& parameterManager) -> bool {
    checkLayoutUpdate(parameterManager);
    return dispatchPenDown(input, parameterManager);
}

auto UseCaseRouter::onPenMotion(const InteractionInput& input, const CurrentState& current,
                                ParameterManager& parameterManager) -> bool {
    checkLayoutUpdate(parameterManager);
    activateUseCases(input, parameterManager, ActivationTrigger::PenMotion);
    return dispatchPenMotion(input, current, parameterManager);
}

auto UseCaseRouter::onPenUp(const InteractionInput& input, ParameterManager& parameterManager) -> bool {
    checkLayoutUpdate(parameterManager);
    return dispatchPenUp(input, parameterManager);
}

void UseCaseRouter::update(const InteractionInput& input, ParameterManager& parameterManager) {
    checkLayoutUpdate(parameterManager);
    const SelectionMode currentMode = parameterManager.getSelectionMode(input.time());
    if (currentMode != lastSelectionMode_) {
        LOG_INFO("SelectionMode", "changed", "from", static_cast<int>(lastSelectionMode_), "to",
                 static_cast<int>(currentMode));
        lastSelectionMode_ = currentMode;
        terminateAll(parameterManager);
        std::lock_guard<std::recursive_mutex> lock(selectionMutex_);
        currentSelection_.clear();
        handleSelectionChanged(parameterManager);
    }
    activateUseCases(input, parameterManager, ActivationTrigger::Any);
}

void UseCaseRouter::onDraw(OverlayRenderer& overlayRenderer, const CurrentState& current,
                           ParameterManager& parameterManager) {
    checkLayoutUpdate(parameterManager);
    dispatchEvents(
        parameterManager, [&](ActiveUseCase& active, const std::vector<std::string_view>& names) -> UseCaseResult {
            return active.useCase_->onDraw(overlayRenderer, active.snapshot_,
                                           currentWithIntents(current, *active.useCase_, mergedIntents_), names);
        });
    for (auto* uc : activeKeyUseCases_) {
        uc->onDraw(overlayRenderer, current);
    }
}

auto UseCaseRouter::makeSnapshot(const InteractionInput& input,
                                 ParameterManager& parameterManager) const -> SnapshotState {
    const OfxPointD mouseOfx = {input.mousePos().x, input.mousePos().y};
    return StateFactory::createSnapshot(input.time(), input.canvasSize().w, input.canvasSize().h,
                                        {input.renderScale().x, input.renderScale().y}, input.mousePos(), mouseOfx,
                                        parameterManager.getSelectionMode(input.time()), getCurrentSelection(),
                                        input.modifiers(), parameterManager);
}

auto UseCaseRouter::onKeyDown(const InteractionInput& input, ParameterManager& parameterManager) -> bool {
    checkLayoutUpdate(parameterManager);
    activateUseCases(input, parameterManager, ActivationTrigger::Any);
    const bool wasEmpty = activeKeyUseCases_.empty();
    const SnapshotState snapshot = makeSnapshot(input, parameterManager);
    activeKeyUseCases_.clear();
    for (const auto& uc : keyUseCases_) {
        if (uc->canHandle(input, snapshot)) {
            activeKeyUseCases_.push_back(uc.get());
        }
    }
    const bool activeChanged = wasEmpty != activeKeyUseCases_.empty();
    bool handled = false;
    for (auto* uc : activeKeyUseCases_) {
        if (uc->onKeyDown(input, snapshot, parameterManager)) {
            handled = true;
        }
    }
    return handled || activeChanged;
}

auto UseCaseRouter::onKeyRepeat(const InteractionInput& input, ParameterManager& parameterManager) -> bool {
    checkLayoutUpdate(parameterManager);
    const SnapshotState snapshot = makeSnapshot(input, parameterManager);
    activeKeyUseCases_.clear();
    for (const auto& uc : keyUseCases_) {
        if (uc->canHandle(input, snapshot)) {
            activeKeyUseCases_.push_back(uc.get());
        }
    }
    bool handled = false;
    for (auto* uc : activeKeyUseCases_) {
        if (uc->onKeyRepeat(input, snapshot, parameterManager)) {
            handled = true;
        }
    }
    return handled;
}

auto UseCaseRouter::onKeyUp(const InteractionInput& input, ParameterManager& parameterManager) -> bool {
    checkLayoutUpdate(parameterManager);
    activateUseCases(input, parameterManager, ActivationTrigger::Any);
    const bool wasEmpty = activeKeyUseCases_.empty();
    const SnapshotState snapshot = makeSnapshot(input, parameterManager);
    activeKeyUseCases_.clear();
    for (const auto& uc : keyUseCases_) {
        if (uc->canHandle(input, snapshot)) {
            activeKeyUseCases_.push_back(uc.get());
        }
    }
    const bool activeChanged = wasEmpty != activeKeyUseCases_.empty();
    bool handled = false;
    for (auto* uc : activeKeyUseCases_) {
        if (uc->onKeyUp(input, snapshot, parameterManager)) {
            handled = true;
        }
    }
    return handled || activeChanged;
}

void UseCaseRouter::applyIntents(const std::string& useCaseName, const InteractionUseCase& useCase,
                                 const IntentMap& intents) {
    const auto declaredEmitted = useCase.declaredEmittedIntents();
    for (const auto& [key, value] : intents) {
        const bool isDeclared = std::ranges::find(declaredEmitted, std::string_view{key}) != declaredEmitted.end();
        if (!isDeclared) {
            LOG_ERROR("Intent", "undeclared_emit", key, "useCase", useCaseName);
            continue;
        }
        if (value.has_value()) {
            mergedIntents_.emit(useCaseName, key, *value);
        } else {
            mergedIntents_.clear(useCaseName, key);
        }
    }
    logIntents();
}

auto UseCaseRouter::snapshotWithIntents(const SnapshotState& snapshot, const InteractionUseCase& useCase,
                                        const IntentRegistry& intents) -> SnapshotState {
    const auto declared = useCase.declaredConsumedIntents();
    SnapshotState result = snapshot;
    const auto& registry = intents.getRegistry();
    for (const auto& key : declared) {
        const auto iterator = registry.find(std::string(key));
        if (iterator != registry.end()) {
            for (const auto& [owner, values] : iterator->second) {
                result.intents_.emit(owner, std::string(key), values);
            }
        }
    }
    return result;
}

auto UseCaseRouter::currentWithIntents(const CurrentState& current, const InteractionUseCase& useCase,
                                       const IntentRegistry& intents) -> CurrentState {
    const auto declared = useCase.declaredConsumedIntents();
    CurrentState result = current;
    const auto& registry = intents.getRegistry();
    for (const auto& key : declared) {
        const auto iterator = registry.find(std::string(key));
        if (iterator != registry.end()) {
            for (const auto& [owner, values] : iterator->second) {
                result.intents_.emit(owner, std::string(key), values);
            }
        }
    }
    return result;
}

void UseCaseRouter::clearIntents(const std::string& useCaseName) {
    mergedIntents_.clear(useCaseName);
    logIntents();
}

void UseCaseRouter::logIntents() const {
    const auto& registry = mergedIntents_.getRegistry();
    int activeKeysCount = 0;
    for (const auto& [key, owners] : registry) {
        bool hasValue = false;
        for (const auto& [owner, values] : owners) {
            if (!values.empty()) {
                hasValue = true;
                break;
            }
        }
        if (hasValue) {
            activeKeysCount++;
        }
    }

    LOG_INFO("Intents", "changed", "count", activeKeysCount);
    int index = 0;
    for (const auto& [key, owners] : registry) {
        std::string mergedValueStr;
        std::string finalType = "other";
        bool firstElement = true;

        for (const auto& [owner, values] : owners) {
            for (const auto& val : values) {
                if (!firstElement) {
                    mergedValueStr += ", ";
                }
                firstElement = false;

                if (const auto* b = std::get_if<bool>(&val)) {
                    mergedValueStr += (*b ? "true" : "false");
                    finalType = "bool";
                } else if (const auto* d = std::get_if<double>(&val)) {
                    mergedValueStr += std::to_string(*d);
                    finalType = "double";
                } else if (const auto* s = std::get_if<SelectionItem>(&val)) {
                    if (s->characterIndex_ == kAllIndices && s->partIndex_ == kAllIndices) {
                        mergedValueStr += "All";
                    } else if (s->partIndex_ == kAllIndices) {
                        mergedValueStr += "char[" + std::to_string(s->characterIndex_) + "]";
                    } else {
                        mergedValueStr += "char[" + std::to_string(s->characterIndex_) + "] part[" +
                                          std::to_string(s->partIndex_) + "]";
                    }
                    finalType = "selection";
                } else {
                    mergedValueStr += "other";
                }
            }
        }

        if (!firstElement) {
            if (finalType == "bool") {
                const bool boolVal = (mergedValueStr == "true");
                LOG_INFO("Intents", "item", "index", index, "key", key, "owner", "System", "type", "bool", "val",
                         boolVal);
            } else if (finalType == "double") {
                double doubleVal = 0.0;
                try {
                    doubleVal = std::stod(mergedValueStr);
                } catch (...) {
                }
                LOG_INFO("Intents", "item", "index", index, "key", key, "owner", "System", "type", "double", "val",
                         doubleVal);
            } else {
                LOG_INFO("Intents", "item", "index", index, "key", key, "owner", "System", "type", mergedValueStr);
            }
            index++;
        }
    }
}

auto UseCaseRouter::dispatchPenDown(const InteractionInput& input, ParameterManager& parameterManager) -> bool {
    activateUseCases(input, parameterManager, ActivationTrigger::PenDown);
    const bool result = dispatchEvents(
        parameterManager, [&](ActiveUseCase& active, const std::vector<std::string_view>& names) -> UseCaseResult {
            return active.useCase_->penDown(input, active.snapshot_, parameterManager, names);
        });
    activateUseCases(input, parameterManager, ActivationTrigger::PenDown);
    return result;
}

auto UseCaseRouter::dispatchPenMotion(const InteractionInput& input, const CurrentState& current,
                                      ParameterManager& parameterManager) -> bool {
    return dispatchEvents(parameterManager, [&](ActiveUseCase& active, const std::vector<std::string_view>& names) {
        return active.useCase_->penMotion(input, active.snapshot_,
                                          currentWithIntents(current, *active.useCase_, mergedIntents_),
                                          parameterManager, names);
    });
}

auto UseCaseRouter::dispatchPenUp(const InteractionInput& input, ParameterManager& parameterManager) -> bool {
    return dispatchEvents(parameterManager, [&](ActiveUseCase& active, const std::vector<std::string_view>& names) {
        return active.useCase_->penUp(input, active.snapshot_, parameterManager, names);
    });
}

auto UseCaseRouter::applySelectionResult(const UseCaseResult& result, ActiveUseCase& active,
                                         ParameterManager& parameterManager) -> bool {
    if (result.targetOperation_.has_value()) {
        const SelectionItem target = active.useCase_->targetSelectionItem();
        const std::string name(active.useCase_->name());
        if (*result.targetOperation_ == TargetItemOperation::Add) {
            std::erase_if(currentSelection_, [&](const auto& osi) {
                return osi.item_.characterIndex_ == target.characterIndex_ && osi.item_.partIndex_ == target.partIndex_;
            });
            currentSelection_.insert(currentSelection_.begin(), {target, name});
        } else if (*result.targetOperation_ == TargetItemOperation::Remove) {
            std::erase_if(currentSelection_, [&](const auto& osi) {
                return osi.item_.characterIndex_ == target.characterIndex_ && osi.item_.partIndex_ == target.partIndex_;
            });
        } else if (*result.targetOperation_ == TargetItemOperation::RemoveIfSelf) {
            auto it = std::find_if(currentSelection_.begin(), currentSelection_.end(), [&](const auto& osi) {
                return osi.item_.characterIndex_ == target.characterIndex_ && osi.item_.partIndex_ == target.partIndex_;
            });
            if (it != currentSelection_.end() && it->ownerName_ == name) {
                currentSelection_.erase(it);
                LOG_INFO("UseCaseLifecycle", "stop", name);
                active.useCase_->onFinish(active.snapshot_, parameterManager);
                clearIntents(name);
                return true;
            }
        }
    } else if (result.replacementSelection_.has_value()) {
        const std::string name(active.useCase_->name());
        currentSelection_.clear();
        for (const auto& item : *result.replacementSelection_) {
            currentSelection_.push_back({item, name});
        }
    }
    return false;
}

auto UseCaseRouter::dispatchEvents(ParameterManager& parameterManager, const EventDispatcher& dispatcher) -> bool {
    std::lock_guard<std::recursive_mutex> lock(selectionMutex_);
    bool isHandled = false;
    bool selectionChanged = false;
    const std::vector<std::string_view> activeNames = getActiveUseCaseNames();
    std::erase_if(currentUseCases_, [&](auto& active) {
        active.snapshot_ = snapshotWithIntents(active.snapshot_, *active.useCase_, mergedIntents_);
        const UseCaseResult result = dispatcher(active, activeNames);
        isHandled = true;

        if (applySelectionResult(result, active, parameterManager)) {
            selectionChanged = true;
            return true;
        }
        if (result.targetOperation_.has_value() || result.replacementSelection_.has_value()) {
            selectionChanged = true;
        }

        if (!result.intents_.empty()) {
            applyIntents(std::string(active.useCase_->name()), *active.useCase_, result.intents_);
        }

        if (result.lifecycle_ == UseCaseLifecycle::Terminate) {
            LOG_INFO("UseCaseLifecycle", "stop", active.useCase_->name());
            active.useCase_->onFinish(active.snapshot_, parameterManager);
            clearIntents(std::string(active.useCase_->name()));
            return true;
        }
        return false;
    });

    if (selectionChanged) {
        handleSelectionChanged(parameterManager);
    }

    return isHandled;
}

void UseCaseRouter::handleSelectionChanged(ParameterManager& parameterManager) {
    LOG_INFO("Selection", "changed", "count", static_cast<int>(currentSelection_.size()));
    for (int i = 0; i < static_cast<int>(currentSelection_.size()); ++i) {
        LOG_INFO("Selection", "item", "index", i, "charIndex", currentSelection_[i].item_.characterIndex_, "partIndex",
                 currentSelection_[i].item_.partIndex_);
    }
    const std::vector<std::string_view> activeNames = getActiveUseCaseNames();
    const auto selection = getCurrentSelection();
    std::erase_if(currentUseCases_, [&](auto& active) {
        if (active.useCase_->onSelectionChanged(selection, activeNames) == UseCaseLifecycle::Terminate) {
            LOG_INFO("UseCaseLifecycle", "stop", active.useCase_->name());
            active.useCase_->onFinish(active.snapshot_, parameterManager);
            clearIntents(std::string(active.useCase_->name()));
            return true;
        }
        return false;
    });
}

auto UseCaseRouter::getCurrentSelection() const -> std::vector<SelectionItem> {
    std::lock_guard<std::recursive_mutex> lock(selectionMutex_);
    std::vector<SelectionItem> selection;
    selection.reserve(currentSelection_.size());
    std::transform(currentSelection_.begin(), currentSelection_.end(), std::back_inserter(selection),
                   [](const OwnedSelectionItem& osi) { return osi.item_; });
    return selection;
}

void UseCaseRouter::updateSelection(const std::vector<SelectionItem>& selection, ParameterManager& parameterManager) {
    checkLayoutUpdate(parameterManager);
    std::lock_guard<std::recursive_mutex> lock(selectionMutex_);
    const bool isDifferent = !std::ranges::equal(
        currentSelection_, selection, [](const OwnedSelectionItem& a, const SelectionItem& b) -> bool {
            return a.item_.characterIndex_ == b.characterIndex_ && a.item_.partIndex_ == b.partIndex_;
        });

    if (isDifferent) {
        currentSelection_.clear();
        for (const auto& item : selection) {
            currentSelection_.push_back({item, "Parameter"});
        }
        handleSelectionChanged(parameterManager);
    }
}

void UseCaseRouter::terminateAll(ParameterManager& parameterManager) {
    for (auto& active : currentUseCases_) {
        LOG_INFO("UseCaseLifecycle", "stop", active.useCase_->name());
        active.useCase_->onFinish(active.snapshot_, parameterManager);
        clearIntents(std::string(active.useCase_->name()));
    }
    currentUseCases_.clear();
}

auto UseCaseRouter::hasActiveUseCases() const -> bool {
    return !currentUseCases_.empty();
}

void UseCaseRouter::clearCharacterFactories() {
    characterPassiveFactories_.clear();
    characterActiveFactories_.clear();
    partPassiveFactories_.clear();
    partActiveFactories_.clear();
}

void UseCaseRouter::rebuildCharacterFactories() {
    clearCharacterFactories();
    // Rebuilding character factories is stubbed out.
}

namespace {

template <typename FactoryType>
void collectCandidatesFromList(const std::vector<FactoryType>& factoryList, const InteractionInput& input,
                               const SnapshotState& currentSnapshot, ActivationTrigger trigger,
                               const std::vector<std::string_view>& activeNames,
                               std::vector<UseCaseRouter::PotentialUseCase>& candidates,
                               const IntentRegistry& mergedIntents) {
    for (const auto& factory : factoryList) {
        std::unique_ptr<InteractionUseCase> useCase = factory();
        if (useCase->activationTrigger() != trigger && useCase->activationTrigger() != ActivationTrigger::Any) {
            continue;
        }

        std::string name(useCase->name());
        if (std::any_of(candidates.begin(), candidates.end(),
                        [&](const auto& candidate) { return candidate.useCase_->name() == name; })) {
            continue;
        }

        SnapshotState snapshot = UseCaseRouter::snapshotWithIntents(currentSnapshot, *useCase, mergedIntents);
        HandlingRole role = useCase->canHandle(input, snapshot, nullptr, activeNames);
        if (role != HandlingRole::None) {
            snapshot.handlingRole_ = role;
            candidates.push_back({std::move(useCase), std::move(snapshot), role, false});
        }
    }
}

}  // namespace

void UseCaseRouter::activateUseCases(const InteractionInput& input, ParameterManager& parameterManager,
                                     ActivationTrigger trigger) {
    const OfxPointD mouseOfx = {input.mousePos().x, input.mousePos().y};
    const SnapshotState currentSnapshot = StateFactory::createSnapshot(
        input.time(), input.canvasSize().w, input.canvasSize().h, {input.renderScale().x, input.renderScale().y},
        input.mousePos(), mouseOfx, parameterManager.getSelectionMode(input.time()), getCurrentSelection(),
        input.modifiers(), parameterManager);

    std::vector<PotentialUseCase> candidates;
    const std::vector<std::string_view> activeNames = getActiveUseCaseNames();

    // 1. Collect from existing active use cases
    for (auto& active : currentUseCases_) {
        HandlingRole role =
            active.useCase_->canHandle(input, snapshotWithIntents(currentSnapshot, *active.useCase_, mergedIntents_),
                                       &active.snapshot_, activeNames);
        candidates.push_back({std::move(active.useCase_), std::move(active.snapshot_), role, true});
    }
    currentUseCases_.clear();

    // 2. Collect new candidates from factories based on trigger and SelectionMode
    const SelectionMode selectionMode = currentSnapshot.selectionMode_;

    if (trigger != ActivationTrigger::PenMotion) {
        if (selectionMode == SelectionMode::Character) {
            collectCandidatesFromList(characterPassiveFactories_, input, currentSnapshot, trigger, activeNames,
                                      candidates, mergedIntents_);
            if (trigger == ActivationTrigger::PenDown) {
                collectCandidatesFromList(characterActiveFactories_, input, currentSnapshot, trigger, activeNames,
                                          candidates, mergedIntents_);
            }
        } else if (selectionMode == SelectionMode::Part) {
            collectCandidatesFromList(partPassiveFactories_, input, currentSnapshot, trigger, activeNames, candidates,
                                      mergedIntents_);
            if (trigger == ActivationTrigger::PenDown) {
                collectCandidatesFromList(partActiveFactories_, input, currentSnapshot, trigger, activeNames,
                                          candidates, mergedIntents_);
            }
        }
    }

    // Always evaluate global factories last (fallback/lower priority than character/part specific selections)
    collectCandidatesFromList(factories_, input, currentSnapshot, trigger, activeNames, candidates, mergedIntents_);

    InteractionUseCase* winnerPrimary = findWinnerPrimary(candidates);
    std::type_index primaryType = (winnerPrimary != nullptr) ? typeid(*winnerPrimary) : typeid(void);

    // Final selection and activation
    for (auto& candidate : candidates) {
        bool keep = false;

        if (candidate.role_ == HandlingRole::Passive) {
            if (dynamic_cast<PassiveUseCase*>(candidate.useCase_.get()) != nullptr) {
                keep = true;
            } else {
                LOG_ERROR("UseCaseRouter", "invalid_passive_inheritance", candidate.useCase_->name());
            }
        } else if (candidate.role_ == HandlingRole::Primary) {
            if (candidate.useCase_.get() == winnerPrimary) {
                keep = true;
            }
        } else if (candidate.role_ == HandlingRole::Secondary) {
            InteractionUseCase* useCasePointer = candidate.useCase_.get();
            if (winnerPrimary != nullptr && typeid(*useCasePointer) == primaryType) {
                keep = true;
            }
        } else if (candidate.role_ == HandlingRole::SecondaryIfOtherOwned) {
            InteractionUseCase* useCasePointer = candidate.useCase_.get();
            if (winnerPrimary != nullptr && typeid(*useCasePointer) == primaryType) {
                const SelectionItem target = useCasePointer->targetSelectionItem();
                const std::string name(useCasePointer->name());
                auto selectionIterator = std::find_if(
                    currentSelection_.begin(), currentSelection_.end(), [&](const auto& ownedSelectionItem) {
                        return ownedSelectionItem.item_.characterIndex_ == target.characterIndex_ &&
                               ownedSelectionItem.item_.partIndex_ == target.partIndex_;
                    });
                if (selectionIterator != currentSelection_.end() && selectionIterator->ownerName_ == name) {
                    // We own this selection but didn't win Primary — remove selection and don't activate
                    currentSelection_.erase(selectionIterator);
                    handleSelectionChanged(parameterManager);
                } else {
                    // Someone else owns it or unowned — activate as Secondary
                    candidate.role_ = HandlingRole::Secondary;
                    candidate.snapshot_.handlingRole_ = HandlingRole::Secondary;
                    keep = true;
                }
            }
        }

        if (keep) {
            if (!candidate.isAlreadyActive_) {
                candidate.snapshot_ = snapshotWithIntents(candidate.snapshot_, *candidate.useCase_, mergedIntents_);
                LOG_INFO("UseCaseLifecycle", "start", candidate.useCase_->name());
                candidate.useCase_->onStart(candidate.snapshot_, parameterManager);
            }
            currentUseCases_.push_back({std::move(candidate.useCase_), std::move(candidate.snapshot_)});
        } else {
            if (candidate.isAlreadyActive_) {
                LOG_INFO("UseCaseLifecycle", "finish", candidate.useCase_->name());
                candidate.useCase_->onFinish(candidate.snapshot_, parameterManager);
                clearIntents(std::string(candidate.useCase_->name()));
            }
        }
    }
}

auto UseCaseRouter::findWinnerPrimary(const std::vector<PotentialUseCase>& candidates) -> InteractionUseCase* {
    InteractionUseCase* winner = nullptr;
    for (const auto& c : candidates) {
        if (c.role_ == HandlingRole::Primary) {
            winner = c.useCase_.get();
            break;
        }
    }
    return winner;
}

auto UseCaseRouter::getActiveUseCaseNames() const -> std::vector<std::string_view> {
    std::vector<std::string_view> names;
    names.reserve(currentUseCases_.size());
    for (const auto& active : currentUseCases_) {
        names.push_back(active.useCase_->name());
    }
    return names;
}

auto UseCaseRouter::isCommandTrigger(std::string_view parameterId) const -> bool {
    return commandTriggerIds_.contains(parameterId);
}

void UseCaseRouter::onTriggerCommand(std::string_view parameterId, double time, ParameterManager& parameterManager) {
    if (!isCommandTrigger(parameterId)) {
        return;
    }
    checkLayoutUpdate(parameterManager);
    std::lock_guard<std::recursive_mutex> lock(selectionMutex_);

    double canvasWidth = 1920.0;
    double canvasHeight = 1080.0;

    for (const auto& command : commandUseCases_) {
        if (command->targetParameterIds().contains(parameterId)) {
            SnapshotState snapshot = StateFactory::createSnapshot(
                time, canvasWidth, canvasHeight, {1.0, 1.0}, {0.0, 0.0}, {0.0, 0.0},
                parameterManager.getSelectionMode(time), getCurrentSelection(), 0, parameterManager);

            command->execute(snapshot, parameterManager, parameterId);
        }
    }
}

void UseCaseRouter::syncLayout(ParameterManager& parameterManager) {
    checkLayoutUpdate(parameterManager);
}

void UseCaseRouter::checkLayoutUpdate(ParameterManager& /*parameterManager*/) {
    // Layout tracking is stubbed out.
}

void UseCaseRouter::cleanupInvalidSelection(ParameterManager& /*parameterManager*/) {
    // Selection validation is stubbed out.
}

}  // namespace MugLab::OfxBase
