#pragma once

#include <string_view>

namespace MugLab::OfxBase {

class OverlayRenderer;
class InteractionInput;
class CurrentState;
class ParameterManager;
class SnapshotState;

/**
 * @brief Base class for key-driven interactions.
 * Registered permanently in UseCaseRouter; canHandle gates all dispatch.
 */
class KeyUseCase {
   public:
    virtual ~KeyUseCase() = default;

    [[nodiscard]] virtual auto name() const -> std::string_view = 0;

    [[nodiscard]] virtual auto canHandle(const InteractionInput& input,
                                         const SnapshotState& snapshot) const -> bool = 0;

    virtual auto onKeyDown(const InteractionInput& input, const SnapshotState& snapshot,
                           ParameterManager& parameterManager) -> bool;
    virtual auto onKeyRepeat(const InteractionInput& input, const SnapshotState& snapshot,
                             ParameterManager& parameterManager) -> bool;
    virtual auto onKeyUp(const InteractionInput& input, const SnapshotState& snapshot,
                         ParameterManager& parameterManager) -> bool;

    virtual void onDraw(OverlayRenderer& renderer, const CurrentState& current);
};

}  // namespace MugLab::OfxBase
