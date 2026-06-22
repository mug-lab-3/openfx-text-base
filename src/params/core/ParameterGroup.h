#pragma once

#include <memory>

#include "GroupParameter.h"
#include "ParameterIndexerBase.h"

namespace MugLab::OfxBase {

// Index over a parameter tree rooted at a single GroupParameter.
class ParameterGroup : public ParameterIndexerBase {
   public:
    void setTree(std::unique_ptr<GroupParameter> root);

    [[nodiscard]] auto root() const -> GroupParameter*;

   private:
    std::unique_ptr<GroupParameter> ownedRoot_;
};

}  // namespace MugLab::OfxBase
