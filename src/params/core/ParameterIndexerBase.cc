#include "ParameterIndexerBase.h"

#include <algorithm>
#include <variant>

#include "BooleanParameter.h"
#include "ChoiceParameter.h"
#include "GroupParameter.h"

namespace MugLab::OfxBase {

static auto evaluateVisibilityConditionForNode(ParameterNode* node, const VisibilityCondition& cond) -> bool {
    if (auto* boolean = dynamic_cast<BooleanParameter*>(node)) {
        if (boolean->isAnimating()) {
            return true;
        }
        bool value = false;
        if (boolean->raw() != nullptr) {
            boolean->raw()->getValue(value);
        }
        const auto* expected = std::get_if<bool>(&cond.expected_);
        return (expected != nullptr) && (value == *expected);
    }
    if (auto* choice = dynamic_cast<ChoiceParameter*>(node)) {
        if (choice->isAnimating()) {
            return true;
        }
        int expectedIndex = -1;
        if (const auto* index = std::get_if<int>(&cond.expected_)) {
            expectedIndex = *index;
        } else if (const auto* option = std::get_if<std::string>(&cond.expected_)) {
            const auto& options = choice->options();
            const auto it = std::ranges::find(options, *option);
            expectedIndex = (it != options.end()) ? static_cast<int>(std::distance(options.begin(), it)) : -1;
        }
        int value = 0;
        if (choice->raw() != nullptr) {
            choice->raw()->getValue(value);
        }
        return value == expectedIndex;
    }
    return false;
}

void ParameterIndexerBase::indexNode(ParameterNode& node) {
    topLevel_.push_back(&node);
    indexDescendants(node);
}

void ParameterIndexerBase::indexDescendants(ParameterNode& node) {
    findById_[node.id()] = &node;
    if (auto* group = dynamic_cast<GroupParameter*>(&node)) {
        for (const auto& child : group->children()) {
            indexDescendants(*child);
        }
    }
}

auto ParameterIndexerBase::findNode(const std::string& id) const -> ParameterNode* {
    auto it = findById_.find(id);
    return it != findById_.end() ? it->second : nullptr;
}

void ParameterIndexerBase::registerVisibilityRule(const std::string& targetId, VisibilityRule rule) {
    pendingRules_.push_back({targetId, std::move(rule)});
}

void ParameterIndexerBase::resolveVisibilityRules() {
    for (auto& pending : pendingRules_) {
        auto* target = findNode(pending.targetId_);
        if (target != nullptr) {
            visibilityRules_.push_back({target, std::move(pending.rule_)});
        }
    }
    pendingRules_.clear();
}

void ParameterIndexerBase::applyVisibilityRules() {
    for (const auto& visibilityEntry : visibilityRules_) {
        if (visibilityEntry.target_->isAnimating()) {
            visibilityEntry.target_->setVisible(true);
            continue;
        }
        const auto& rule = visibilityEntry.rule_;
        bool visible = (rule.logic_ == VisibilityRule::Logic::kAnd);
        for (const auto& cond : rule.conditions_) {
            bool condResult = evaluateVisibilityConditionForNode(findNode(cond.paramId_), cond);
            if (rule.logic_ == VisibilityRule::Logic::kAnd) {
                visible = visible && condResult;
            } else {
                visible = visible || condResult;
            }
        }
        visibilityEntry.target_->setVisible(visible);
    }
}

void ParameterIndexerBase::reset(bool deleteAnimation, double time) {
    for (auto* node : topLevel_) {
        node->reset(deleteAnimation, time);
    }
}

void ParameterIndexerBase::deleteKeysInRange(double lo, double hi) {
    for (auto* node : topLevel_) {
        node->deleteKeysInRange(lo, hi);
    }
}

void ParameterIndexerBase::setKeyFrameAtTime(double time) {
    for (auto* node : topLevel_) {
        node->setKeyFrameAtTime(time);
    }
}

void ParameterIndexerBase::setVisibility(const std::string& id, bool visible) {
    if (auto* node = findNode(id)) {
        node->setVisible(visible);
    }
}

}  // namespace MugLab::OfxBase
