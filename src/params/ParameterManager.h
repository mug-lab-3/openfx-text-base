#pragma once

#include <string>

#include "core/RootParameterGroup.h"
#include "core/Types.h"
#include "interaction/SelectionItem.h"
#include "ofxsImageEffect.h"

namespace MugLab::OfxBase {

// ---------------------------------------------------------------------------
// Default parameter values — single source of truth shared by parameter
// declarations and command/key use cases that reset values to defaults.
// ---------------------------------------------------------------------------
inline constexpr ParamPoint2D kDefaultPosition{.x_ = 0.5, .y_ = 0.5};
inline constexpr double kDefaultSize = 1.0;
inline constexpr double kDefaultFontSize = 40.0;

// ParameterManager owns the full parameter tree for one plugin instance.
// Usage:
//   describe-time : ParameterManager::defineAllParameters(descriptor)
//   instance-time : ParameterManager pm(effectInstance); pm.updateVisibility(time);
class ParameterManager {
   public:
    // Called once at describe-time (no instance exists).
    static void defineAllParameters(OFX::ImageEffectDescriptor& descriptor);

    // Called at instance-time inside the plugin constructor.
    explicit ParameterManager(OFX::ImageEffect& effect);

    // Re-evaluate all visibility rules and push show/hide to the host.
    // Call from changedParam() — skip when args.reason == eChangeTime.
    void updateVisibility(double time);

    // ---------------------------------------------------------------------------
    // Value getters — one per value type, identified by ParamIds constant.
    // Returns fallback when the parameter is not found.
    // ---------------------------------------------------------------------------
    [[nodiscard]] auto getString(const std::string& id, double time,
                                 const std::string& fallback = {}) const -> std::string;
    [[nodiscard]] auto getDouble(const std::string& id, double time, double fallback = 0.0) const -> double;
    [[nodiscard]] auto getDouble2D(const std::string& id, double time,
                                   const ParamPoint2D& fallback = {}) const -> ParamPoint2D;
    [[nodiscard]] auto getBool(const std::string& id, double time, bool fallback = false) const -> bool;
    [[nodiscard]] auto getChoice(const std::string& id, double time, int fallback = 0) const -> int;
    [[nodiscard]] auto getRGBA(const std::string& id, double time, const ParamRGBA& fallback = {}) const -> ParamRGBA;

    [[nodiscard]] auto getSelectionMode(double time) const -> SelectionMode;

    // Convenience: resolves a ChoiceParameter index to its option string.
    [[nodiscard]] auto getChoiceOption(const std::string& id, double time,
                                       const std::string& fallback = {}) const -> std::string;

    // Value setters
    void setDouble(const std::string& id, double value, double time);
    void setDouble2D(const std::string& id, const ParamPoint2D& value, double time);

    // Edit blocks
    void beginEditBlock(const std::string& name);
    void endEditBlock();

    // Direct parameter node access for advanced use (e.g. slot-based GroupParameter lookup).
    template <typename T>
    [[nodiscard]] auto findParameter(const std::string& id) const -> T* {
        return root_.findParameter<T>(id);
    }

   private:
    RootParameterGroup root_;
    OFX::ImageEffect* effect_ = nullptr;
};

}  // namespace MugLab::OfxBase
