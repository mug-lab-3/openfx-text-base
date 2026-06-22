#pragma once

#include "interaction/InteractionUseCase.h"

namespace MugLab::OfxBase {

/**
 * @brief Base class for passive use cases that should not have access to ParameterManager.
 *
 * Passive use cases are primarily for visual feedback or internal state updates that do not
 * modify the plugin parameters directly.
 */
class PassiveUseCase : public InteractionUseCase {
   public:
    virtual ~PassiveUseCase() = default;

    // Filter out ParameterManager by providing final implementations that delegate to parameter-free versions.

    auto penDown(const InteractionInput& input, SnapshotState& snapshot, ParameterManager& parameterManager,
                 const std::vector<std::string_view>& activeUseCases) -> UseCaseResult override {
        return onPassivePenDown(input, snapshot, activeUseCases);
    }

    auto penMotion(const InteractionInput& input, SnapshotState& snapshot, const CurrentState& current,
                   ParameterManager& parameterManager,
                   const std::vector<std::string_view>& activeUseCases) -> UseCaseResult override {
        return onPassivePenMotion(input, snapshot, current, activeUseCases);
    }

    auto penUp(const InteractionInput& input, SnapshotState& snapshot, ParameterManager& parameterManager,
               const std::vector<std::string_view>& activeUseCases) -> UseCaseResult override {
        return onPassivePenUp(input, snapshot, activeUseCases);
    }

   protected:
    /**
     * @brief Passive version of penDown.
     */
    virtual auto onPassivePenDown(const InteractionInput& input, SnapshotState& snapshot,
                                  const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
        return {.lifecycle_ = UseCaseLifecycle::Continue};
    }

    /**
     * @brief Passive version of penMotion.
     */
    virtual auto onPassivePenMotion(const InteractionInput& input, SnapshotState& snapshot, const CurrentState& current,
                                    const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
        return {.lifecycle_ = UseCaseLifecycle::Continue};
    }

    /**
     * @brief Passive version of penUp.
     */
    virtual auto onPassivePenUp(const InteractionInput& input, SnapshotState& snapshot,
                                const std::vector<std::string_view>& activeUseCases) -> UseCaseResult {
        return {.lifecycle_ = UseCaseLifecycle::Continue};
    }
};

}  // namespace MugLab::OfxBase
