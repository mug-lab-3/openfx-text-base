#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ParameterNode.h"
#include "VisibilityRule.h"

namespace MugLab::OfxBase {

// Common id-lookup, visibility-rule, and bulk-operation logic shared by ParameterGroup
// (single GroupParameter root) and RootParameterGroup (flat list of top-level nodes).
//
// Subclasses register their nodes via indexNode() (recursing into nested GroupParameters
// and appending each top-level node to topLevel_), then expose tree-shape-specific
// accessors (root(), addChild(), ...) on top of the shared operations below.
//
// Access patterns:
//   findParameter / findValue — recursive tree search with memoized cache, O(1) after first call
class ParameterIndexerBase {
   public:
    virtual ~ParameterIndexerBase() = default;

    template <typename T>
    [[nodiscard]] auto findParameter(const std::string& id) const -> T* {
        return dynamic_cast<T*>(findNode(id));
    }

    template <typename T>
    [[nodiscard]] auto findValue(const std::string& id, double time, const typename T::ValueType& fallback) const ->
        typename T::ValueType {
        const auto* parameter = findParameter<T>(id);
        return (parameter != nullptr) ? parameter->get(time) : fallback;
    }

    // Resets all indexed nodes to their spec defaults. If deleteAnimation is true, removes all keyframes first.
    void reset(bool deleteAnimation, double time);

    // Deletes all keyframes in the range [lo, hi] (inclusive) for all indexed nodes.
    void deleteKeysInRange(double lo, double hi);

    // Forces a keyframe at the given time using the current value for all indexed nodes.
    void setKeyFrameAtTime(double time);

    // Evaluates all registered VisibilityRules and calls setVisible() on affected nodes.
    // setVisible() is a no-op when visibility has not changed, so host API is not called unnecessarily.
    void applyVisibilityRules();

    // Manually overrides the visibility of a node by id.
    // setVisible() is a no-op when visibility has not changed.
    void setVisibility(const std::string& id, bool visible);

    // Registers a VisibilityRule for a node (called by ParameterTreeBuilder after fetch).
    void registerVisibilityRule(const std::string& targetId, VisibilityRule rule);

   protected:
    // Indexes node and, if it is a GroupParameter, recurses into its children.
    // Appends node itself to topLevel_, so call this once per top-level node.
    void indexNode(ParameterNode& node);

    // Resolves any visibility rules registered for nodes that have since been indexed.
    void resolveVisibilityRules();

    [[nodiscard]] auto findNode(const std::string& id) const -> ParameterNode*;

    std::vector<ParameterNode*> topLevel_;

   private:
    struct VisibilityEntry {
        ParameterNode* target_ = nullptr;
        VisibilityRule rule_;
    };

    struct PendingRule {
        std::string targetId_;
        VisibilityRule rule_;
    };

    // Recursively indexes node and, if it is a GroupParameter, all of its descendants.
    void indexDescendants(ParameterNode& node);

    std::unordered_map<std::string, ParameterNode*> findById_;
    std::vector<PendingRule> pendingRules_;
    std::vector<VisibilityEntry> visibilityRules_;
};

}  // namespace MugLab::OfxBase
