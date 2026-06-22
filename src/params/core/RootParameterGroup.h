#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ParameterIndexerBase.h"
#include "ParameterNode.h"
#include "Types.h"
#include "ofxsParam.h"

namespace MugLab::OfxBase {

// Index over a flat list of top-level parameter nodes.
// Unlike ParameterGroup, no OFX GroupParam is created for the root —
// children are owned directly and registered into the OFX host as top-level params.
class RootParameterGroup : public ParameterIndexerBase {
   public:
    void addChild(std::unique_ptr<ParameterNode> child);

    [[nodiscard]] auto findValueParam(const std::string& id) const -> OFX::ValueParam*;
    [[nodiscard]] auto collectAnimatingParamInfos() const -> std::vector<ParamIdTypePair>;

   private:
    std::vector<std::unique_ptr<ParameterNode>> children_;
};

}  // namespace MugLab::OfxBase
