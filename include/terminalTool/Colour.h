/**
 * @file Colour.h
 * @brief Declares RGB colours and the built-in terminalTool palette.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <cstdint>
#include <string>

namespace tt {

/**
 * @brief Stores a 24-bit RGB colour and provides cached ANSI sequences.
 *
 * ANSI foreground and background strings are created only once for each unique
 * RGB value used by the process. Later calls reuse the cached strings.
 */
class Colour {
private:
    std::uint8_t red {};
    std::uint8_t green {};
    std::uint8_t blue {};

public:
    /** @brief Creates black (`0, 0, 0`). */
    constexpr Colour() noexcept = default;

    /**
     * @brief Creates an RGB colour.
     * @param red Red channel in the range 0-255.
     * @param green Green channel in the range 0-255.
     * @param blue Blue channel in the range 0-255.
     */
    constexpr Colour(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
        : red(red), green(green), blue(blue) {}

    /**
     * @brief Gets the cached ANSI true-colour foreground sequence.
     * @return Reference that remains valid for the lifetime of the process.
     */
    [[nodiscard]] const std::string& foreground() const;

    /**
     * @brief Gets the cached ANSI true-colour background sequence.
     * @return Reference that remains valid for the lifetime of the process.
     */
    [[nodiscard]] const std::string& background() const;

    /** @return The red channel. */
    [[nodiscard]] constexpr std::uint8_t getRed() const noexcept { return red; }

    /** @return The green channel. */
    [[nodiscard]] constexpr std::uint8_t getGreen() const noexcept { return green; }

    /** @return The blue channel. */
    [[nodiscard]] constexpr std::uint8_t getBlue() const noexcept { return blue; }

    /** @return `true` when every RGB channel is equal. */
    [[nodiscard]] constexpr bool operator==(const Colour& other) const noexcept {
        return red == other.red && green == other.green && blue == other.blue;
    }

    /** @return `true` when at least one RGB channel differs. */
    [[nodiscard]] constexpr bool operator!=(const Colour& other) const noexcept {
        return !(*this == other);
    }

    /** @brief ANSI sequence that resets terminal attributes. */
    static constexpr const char* RESET = "\033[0m";
};

/**
 * @brief Ready-to-use conventional 16-colour ANSI/VGA-style RGB palette.
 *
 * Terminals are free to use different native 16-colour palettes. These values
 * provide stable RGB equivalents while still using terminalTool true-colour
 * output.
 */
namespace Colours {
    inline constexpr Colour Black { 0, 0, 0 }; ///< Conventional ANSI black.
    inline constexpr Colour Red { 170, 0, 0 }; ///< Conventional ANSI red.
    inline constexpr Colour Green { 0, 170, 0 }; ///< Conventional ANSI green.
    inline constexpr Colour Yellow { 170, 85, 0 }; ///< Conventional ANSI yellow/brown.
    inline constexpr Colour Blue { 0, 0, 170 }; ///< Conventional ANSI blue.
    inline constexpr Colour Magenta { 170, 0, 170 }; ///< Conventional ANSI magenta.
    inline constexpr Colour Cyan { 0, 170, 170 }; ///< Conventional ANSI cyan.
    inline constexpr Colour White { 170, 170, 170 }; ///< Conventional ANSI white/light grey.

    inline constexpr Colour BrightBlack { 85, 85, 85 }; ///< Bright black/dark grey.
    inline constexpr Colour BrightRed { 255, 85, 85 }; ///< Bright red.
    inline constexpr Colour BrightGreen { 85, 255, 85 }; ///< Bright green.
    inline constexpr Colour BrightYellow { 255, 255, 85 }; ///< Bright yellow.
    inline constexpr Colour BrightBlue { 85, 85, 255 }; ///< Bright blue.
    inline constexpr Colour BrightMagenta { 255, 85, 255 }; ///< Bright magenta.
    inline constexpr Colour BrightCyan { 85, 255, 255 }; ///< Bright cyan.
    inline constexpr Colour BrightWhite { 255, 255, 255 }; ///< Bright white.

    inline constexpr Colour Grey = BrightBlack; ///< Grey alias.
    inline constexpr Colour LightGrey = White; ///< Light-grey alias.
    inline constexpr Colour DefaultForeground { 255, 255, 255 }; ///< terminalTool default foreground.
    inline constexpr Colour DefaultBackground { 12, 12, 12 }; ///< terminalTool default background.
}

} // namespace tt
