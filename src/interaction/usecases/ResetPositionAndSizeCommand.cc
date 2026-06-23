#include "interaction/usecases/ResetPositionAndSizeCommand.h"

#include "params/ParamIds.h"
#include "params/ParameterManager.h"

namespace MugLab::OfxBase {

auto ResetPositionAndSizeCommand::name() const -> std::string_view {
    return "ResetPositionAndSizeCommand";
}

auto ResetPositionAndSizeCommand::targetParameterIds() const -> std::unordered_set<std::string_view> {
    return {ParamIds::kResetPositionAndSize};
}

void ResetPositionAndSizeCommand::execute(const SnapshotState& snapshot, ParameterManager& parameterManager,
                                          std::string_view /*triggeredParameter*/) {
    parameterManager.beginEditBlock("Reset Position and Size");
    parameterManager.setDouble2D(ParamIds::kPosition, kDefaultPosition, snapshot.time_);
    parameterManager.setDouble(ParamIds::kFontSize, kDefaultFontSize, snapshot.time_);
    parameterManager.endEditBlock();
}

}  // namespace MugLab::OfxBase
