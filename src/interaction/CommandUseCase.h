#pragma once

#include <string_view>
#include <unordered_set>

#include "interaction/SnapshotState.h"

namespace MugLab::OfxBase {

class ParameterManager;

/**
 * @brief Base class for one-shot actions triggered by parameter changes or UI buttons.
 * Most command use cases are stateless, but a use case may keep state across invocations (e.g. a
 * copy/paste command that remembers a copied selection). Their lifetime is managed by UseCaseRouter,
 * which creates each instance once and keeps it for the router's lifetime.
 */
class CommandUseCase {
   public:
    virtual ~CommandUseCase() = default;

    /**
     * @brief Gets the unique name of this command use case.
     * @return A string view representing the name.
     */
    [[nodiscard]] virtual auto name() const -> std::string_view = 0;

    /**
     * @brief Gets the UI Parameter IDs (buttons) that trigger this command.
     * A use case driven by a single button returns one name; one driven by several buttons (e.g.
     * copy/paste) returns all of them. The triggering button is passed back to execute().
     */
    [[nodiscard]] virtual auto targetParameterIds() const -> std::unordered_set<std::string_view> = 0;

    /**
     * @brief Executes the command action.
     * @param snapshot Frozen snapshot of the system state.
     * @param parameterManager Accessor/Mutator for plugin parameters.
     * @param triggeredParameter The parameter ID whose change triggered this run (the pressed button).
     */
    virtual void execute(const SnapshotState& snapshot, ParameterManager& parameterManager,
                         std::string_view triggeredParameter) = 0;
};

}  // namespace MugLab::OfxBase
