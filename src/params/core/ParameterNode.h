#pragma once

#include <optional>
#include <string>

#include "ofxsImageEffect.h"

namespace MugLab::OfxBase {

// Concept satisfied by any Spec struct that carries the shared base fields.
template <typename T>
concept SpecWithBaseFields = requires(const T& spec) {
    spec.id_;
    spec.label_;
    spec.shortLabel_;
    spec.longLabel_;
    spec.hint_;
    spec.animates_;
};

// Applies the label/hint/animates/parent fields shared by all OFX value param descriptors.
template <SpecWithBaseFields SpecType>
void applyBaseSpec(OFX::ValueParamDescriptor& descriptor, const SpecType& spec,
                   OFX::GroupParamDescriptor* parentGroup) {
    if (!spec.label_.empty() || !spec.shortLabel_.empty() || !spec.longLabel_.empty()) {
        const auto& shortLabel = spec.shortLabel_.empty() ? spec.label_ : spec.shortLabel_;
        const auto& longLabel = spec.longLabel_.empty() ? spec.label_ : spec.longLabel_;
        descriptor.setLabels(spec.label_, shortLabel, longLabel);
    }
    if (!spec.hint_.empty()) {
        descriptor.setHint(spec.hint_);
    }
    if (!spec.animates_) {
        descriptor.setAnimates(false);
    }
    if (parentGroup != nullptr) {
        descriptor.setParent(*parentGroup);
    }
}

// Non-template base for all parameter tree nodes (leaves and groups).
// Provides identity, the define/fetch lifecycle, and a cached setIsSecret wrapper
// so callers can call setVisible() repeatedly without redundant host calls.
class ParameterNode {
   public:
    explicit ParameterNode(std::string id, std::optional<int> slot = std::nullopt);
    virtual ~ParameterNode() = default;

    ParameterNode(const ParameterNode&) = delete;
    auto operator=(const ParameterNode&) -> ParameterNode& = delete;

    // Creates the underlying OFX parameter(s) under the given parent group (may be null for root).
    virtual void define(OFX::ImageEffectDescriptor& descriptor, OFX::GroupParamDescriptor* parentGroup) = 0;

    // Acquires references to the already-instantiated OFX parameter(s).
    virtual void fetch(OFX::ImageEffect& effectInstance) = 0;

    [[nodiscard]] auto id() const -> const std::string&;
    [[nodiscard]] auto slot() const -> std::optional<int>;

    [[nodiscard]] virtual auto isAnimating() const -> bool {
        return false;
    }
    [[nodiscard]] virtual auto rawValueParam() const -> OFX::ValueParam* {
        return nullptr;
    }

    // Resets this node to its spec default. If deleteAnimation is true, removes all keyframes first.
    // time is used to place a keyframe at the current position when the parameter is animating.
    virtual void reset(bool deleteAnimation, double time) = 0;

    // Deletes all keyframes in the range [lo, hi] (inclusive).
    virtual void deleteKeysInRange(double lo, double hi) = 0;

    // Forces a keyframe at the given time using the current value.
    virtual void setKeyFrameAtTime(double time) = 0;

    // Applies setIsSecret only when the visibility actually changes.
    void setVisible(bool visible);

   protected:
    virtual void applyVisible(bool visible) = 0;

   private:
    std::string id_;
    std::optional<int> slot_;
    std::optional<bool> lastVisible_;
};

}  // namespace MugLab::OfxBase
