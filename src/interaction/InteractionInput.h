#pragma once

#include <blend2d.h>

#include <cstdint>

namespace OFX {
struct PenArgs;
struct DrawArgs;
struct KeyArgs;
}  // namespace OFX

namespace MugLab::OfxBase {

/**
 * @brief Bit flags for interaction modifier keys.
 */
enum Modifier : std::uint8_t {
    Mod_None = 0,
    Mod_Ctrl = 1 << 0,
    Mod_Alt = 1 << 1,
    Mod_Shift = 1 << 2
};

// Bitwise operators for Modifier
inline auto operator|(Modifier a, Modifier b) -> Modifier {
    return static_cast<Modifier>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline auto operator&(Modifier a, Modifier b) -> Modifier {
    return static_cast<Modifier>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

inline auto operator~(Modifier a) -> Modifier {
    return static_cast<Modifier>(~static_cast<std::uint8_t>(a));
}

/**
 * @brief InteractionInput wraps OFX interaction arguments and provides a platform-independent input state.
 */
class InteractionInput {
   public:
    /**
     * @brief Constructs an InteractionInput with default empty state.
     */
    InteractionInput();

    /**
     * @brief Constructs an InteractionInput from OFX PenArgs and pre-calculated data.
     */
    InteractionInput(const OFX::PenArgs& args, const BLPoint& convertedMousePos, Modifier modifiers);

    /**
     * @brief Updates the canvas dimensions and basic draw state.
     * Called during the draw action.
     */
    void update(const OFX::DrawArgs& args, double canvasWidth, double canvasHeight);

    /**
     * @brief Updates the input state with new pen event data.
     */
    void update(const OFX::PenArgs& args);

    /**
     * @brief Updates the modifier key state based on key events.
     */
    void update(const OFX::KeyArgs& args, bool isDown);

    /**
     * @brief Updates the state of a specific modifier key.
     */
    void setModifier(Modifier mod, bool isDown);

    /**
     * @brief Gets the current mouse position in Blend2D canvas coordinates (pixels, Y-down: 0=top, height=bottom).
     */
    [[nodiscard]] auto mousePos() const -> const BLPoint&;

    /**
     * @brief Gets the current effect time.
     */
    [[nodiscard]] auto time() const -> double;

    /**
     * @brief Gets the current render scale (from InteractArgs).
     */
    [[nodiscard]] auto renderScale() const -> const BLPoint&;

    /**
     * @brief Gets the current pixel scale (from PenArgs).
     */
    [[nodiscard]] auto pixelScale() const -> const BLPoint&;

    /**
     * @brief Gets the normalized pen pressure.
     */
    [[nodiscard]] auto pressure() const -> double;

    /**
     * @brief Gets the canvas width.
     */
    [[nodiscard]] auto canvasWidth() const -> double;

    /**
     * @brief Gets the canvas height.
     */
    [[nodiscard]] auto canvasHeight() const -> double;

    /**
     * @brief Gets the canvas size.
     */
    [[nodiscard]] auto canvasSize() const -> BLSize;

    /**
     * @brief Checks if the modifier key state exactly matches the given mask.
     */
    [[nodiscard]] auto modifiersMatch(Modifier mask) const -> bool;

    /**
     * @brief Checks if the given modifiers are included in the current state.
     */
    [[nodiscard]] auto hasModifier(Modifier mask) const -> bool;

    /**
     * @brief Convenient check for Ctrl key.
     */
    [[nodiscard]] auto ctrl() const -> bool;

    /**
     * @brief Convenient check for Alt key.
     */
    [[nodiscard]] auto alt() const -> bool;

    /**
     * @brief Convenient check for Shift key.
     */
    [[nodiscard]] auto shift() const -> bool;

    /**
     * @brief Gets the raw modifier bitmask.
     */
    [[nodiscard]] auto modifiers() const -> uint8_t;

    /**
     * @brief Gets the last key symbol from a key event (0 if no key event occurred).
     */
    [[nodiscard]] auto keySymbol() const -> int;

   private:
    double canvasWidth_;   // Canvas width in pixels
    double canvasHeight_;  // Canvas height in pixels
    BLPoint mousePos_;     // Blend2D canvas coordinates (pixels, Y-down: 0=top, height=bottom)
    double time_;          // Current effect time
    BLPoint renderScale_;  // Project-to-render resolution ratio
    BLPoint pixelScale_;   // Pixel aspect ratio
    double pressure_;      // Normalized pen pressure
    Modifier modifiers_;   // Modifier key bitmask
    int keySymbol_ = 0;    // Last key symbol from key event
};

}  // namespace MugLab::OfxBase
