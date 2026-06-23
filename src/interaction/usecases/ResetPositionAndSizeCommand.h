#pragma once

#include "interaction/CommandUseCase.h"

namespace MugLab::OfxBase {

class ResetPositionAndSizeCommand : public CommandUseCase {
   public:
    [[nodiscard]] auto name() const -> std::string_view override;
    [[nodiscard]] auto targetParameterIds() const -> std::unordered_set<std::string_view> override;
    void execute(const SnapshotState& snapshot, ParameterManager& parameterManager,
                 std::string_view triggeredParameter) override;
};

}  // namespace MugLab::OfxBase
