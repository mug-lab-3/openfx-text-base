#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ParameterNode.h"

namespace MugLab::OfxBase {

// Composite node that mirrors OFX::GroupParam nesting. Building the tree (add<T>) is a
// separate phase from instantiating it: define()/fetch() recursively propagate to children
// once a descriptor / effect instance becomes available.
class GroupParameter : public ParameterNode {
   public:
    struct Spec {
        std::string id_;
        std::optional<int> slot_;
        std::string label_;
        std::string shortLabel_;
        std::string longLabel_;
        std::string hint_;
        bool open_ = true;
    };

    explicit GroupParameter(Spec spec);

    static auto create(Spec spec) -> std::unique_ptr<GroupParameter>;

    template <typename T, typename... Args>
        requires std::derived_from<T, ParameterNode>
    auto add(Args&&... args) -> GroupParameter& {
        return addChild(std::make_unique<T>(std::forward<Args>(args)...));
    }

    // Takes ownership of an already-constructed (and fetched) node.
    auto addOwned(std::unique_ptr<ParameterNode> node) -> GroupParameter& {
        return addChild(std::move(node));
    }

    // Direct children only (by id or slot).
    template <typename T>
        requires std::derived_from<T, ParameterNode>
    [[nodiscard]] auto getParameter(const std::string& id) const -> T* {
        auto it = childById_.find(id);
        return it != childById_.end() ? dynamic_cast<T*>(it->second) : nullptr;
    }

    template <typename T>
        requires std::derived_from<T, ParameterNode>
    [[nodiscard]] auto getParameter(int slot) const -> T* {
        auto it = childBySlot_.find(slot);
        return it != childBySlot_.end() ? dynamic_cast<T*>(it->second) : nullptr;
    }

    template <typename T>
        requires std::derived_from<T, ParameterNode>
    [[nodiscard]] auto getValue(const std::string& id, double time, const typename T::ValueType& fallback) const ->
        typename T::ValueType {
        const auto* p = getParameter<T>(id);
        return (p != nullptr) ? p->get(time) : fallback;
    }

    template <typename T>
        requires std::derived_from<T, ParameterNode>
    [[nodiscard]] auto getValue(int slot, double time, const typename T::ValueType& fallback) const ->
        typename T::ValueType {
        const auto* p = getParameter<T>(slot);
        return (p != nullptr) ? p->get(time) : fallback;
    }

    // Recursive tree search. Returns nullptr if absent or wrong type.
    template <typename T>
        requires std::derived_from<T, ParameterNode>
    [[nodiscard]] auto find(const std::string& id) const -> T* {
        auto it = childById_.find(id);
        if (it != childById_.end()) {
            return dynamic_cast<T*>(it->second);
        }
        for (const auto& child : children_) {
            auto* nestedGroup = dynamic_cast<GroupParameter*>(child.get());
            if (nestedGroup != nullptr) {
                auto* result = nestedGroup->find<T>(id);
                if (result != nullptr) {
                    return result;
                }
            }
        }
        return nullptr;
    }

    void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) override;
    void fetch(OFX::ImageEffect& effectInstance) override;
    void reset(bool deleteAnimation, double time) override;
    void deleteKeysInRange(double lo, double hi) override;
    void setKeyFrameAtTime(double time) override;

    [[nodiscard]] auto children() const -> const std::vector<std::unique_ptr<ParameterNode>>&;
    [[nodiscard]] auto group() const -> OFX::GroupParam*;

   protected:
    void applyVisible(bool visible) override;

   private:
    auto addChild(std::unique_ptr<ParameterNode> child) -> GroupParameter&;

    Spec spec_;
    OFX::GroupParam* group_ = nullptr;
    std::vector<std::unique_ptr<ParameterNode>> children_;
    std::unordered_map<std::string, ParameterNode*> childById_;
    std::unordered_map<int, ParameterNode*> childBySlot_;
};

}  // namespace MugLab::OfxBase
