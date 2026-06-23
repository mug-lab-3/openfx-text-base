#pragma once

#include "interaction/KeyUseCase.h"

namespace MugLab::OfxBase {

class ResetPositionAndSizeKeyUseCase : public KeyUseCase {
   public:
    [[nodiscard]] auto name() const -> std::string_view override;
    [[nodiscard]] auto canHandle(const InteractionInput& input,
                                 const SnapshotState& snapshot) const -> bool override;
    auto onKeyDown(const InteractionInput& input, const SnapshotState& snapshot,
                   ParameterManager& parameterManager) -> bool override;
};

}  // namespace MugLab::OfxBase
