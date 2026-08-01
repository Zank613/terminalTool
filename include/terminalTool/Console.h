/**
 * @file Console.h
 * @brief Declares the terminalTool framebuffer renderer and drawing helpers.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "terminalTool/Colour.h"

namespace tt {

/**
 * @brief Static cell-based terminal framebuffer and drawing API.
 *
 * Call beginFrame(), issue drawing commands, and then call endFrame(). The
 * renderer compares the current framebuffer with the previous one and chooses
 * either a complete or differential terminal update.
 */
class Console {
public:
    /** @brief Terminal dimensions measured in character cells. */
    struct Size {
        int width = 120;  ///< Width in character cells.
        int height = 40; ///< Height in character cells.
    };

    /** @brief Integer rectangle measured in character cells. */
    struct Rect {
        int x = 0;      ///< Left coordinate.
        int y = 0;      ///< Top coordinate.
        int width = 0;  ///< Width in character cells.
        int height = 0; ///< Height in character cells.

        /**
         * @brief Tests whether a point lies inside the rectangle.
         * @param pointX Horizontal point coordinate.
         * @param pointY Vertical point coordinate.
         * @return `true` when the point is inside the half-open rectangle.
         */
        [[nodiscard]] bool contains(int pointX, int pointY) const;
    };

    /** @brief Horizontal text alignment inside a rectangle. */
    enum class TextAlignment {
        Left,   ///< Align text with the left edge.
        Centre, ///< Centre text horizontally.
        Right   ///< Align text with the right edge.
    };

    /** @brief Border character set used by drawBox() and drawPanel(). */
    enum class BoxStyle {
        Single, ///< Unicode single-line border.
        Double, ///< Unicode double-line border.
        Ascii   ///< Portable `+`, `-`, and `|` border.
    };

private:
    struct Cell {
        char32_t character = U' ';
        Colour foreground = Colour(255, 255, 255);
        Colour background = Colour(12, 12, 12);

        bool operator==(const Cell& other) const;
        bool operator!=(const Cell& other) const;
    };

    struct BoxCharacters {
        char32_t horizontal;
        char32_t vertical;
        char32_t topLeft;
        char32_t topRight;
        char32_t bottomLeft;
        char32_t bottomRight;
    };

    static int frameWidth;
    static int frameHeight;
    static bool frameBufferActive;
    static bool firstFrame;
    static std::vector<Cell> frameBuffer;
    static std::vector<Cell> previousBuffer;

    static void renderFullFrame();
    static void renderDifferentialFrame();

    [[nodiscard]] static bool isInsideFrame(int x, int y);
    [[nodiscard]] static Rect frameRect();
    [[nodiscard]] static Rect intersect(const Rect& first, const Rect& second);
    [[nodiscard]] static BoxCharacters boxCharacters(BoxStyle style);
    [[nodiscard]] static std::string cursorPosition(int x, int y);
    [[nodiscard]] static std::string encodeUtf8(char32_t character);
    [[nodiscard]] static std::vector<char32_t> decodeUtf8(const std::string& text);

public:
    /**
     * @brief Creates the framebuffer.
     * @param width Width in cells. Values less than one are ignored.
     * @param height Height in cells. Values less than one are ignored.
     */
    static void initializeFrameBuffer(int width, int height);

    /**
     * @brief Recreates the framebuffer at a new size.
     * @param width New width in cells.
     * @param height New height in cells.
     */
    static void resizeFrameBuffer(int width, int height);

    /**
     * @brief Resizes the framebuffer to match the visible terminal.
     * @return `true` when the framebuffer size changed.
     * @throws TerminalError when the terminal dimensions cannot be queried.
     */
    static bool resizeToTerminal();

    /** @brief Releases framebuffer storage and resets renderer state. */
    static void shutdownFrameBuffer();

    /**
     * @return The current visible terminal size.
     * @throws TerminalError when the terminal dimensions cannot be queried.
     */
    [[nodiscard]] static Size terminalSize();

    /** @return Current framebuffer width in cells. */
    [[nodiscard]] static int getFrameWidth();

    /** @return Current framebuffer height in cells. */
    [[nodiscard]] static int getFrameHeight();

    /** @return `true` after framebuffer initialization and before shutdown. */
    [[nodiscard]] static bool isActive();

    /**
     * @brief Clears the current framebuffer with one foreground/background pair.
     * @param foreground Default foreground colour for empty cells.
     * @param background Default background colour for empty cells.
     */
    static void beginFrame(Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12));

    /**
     * @brief Draws one Unicode code point into a framebuffer cell.
     * @param x Horizontal cell coordinate.
     * @param y Vertical cell coordinate.
     * @param character Unicode code point to draw.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawCell(int x, int y, char32_t character, Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12));

    /**
     * @brief Draws UTF-8 text clipped to the framebuffer.
     * @param x Starting horizontal coordinate.
     * @param y Row coordinate.
     * @param text UTF-8 text.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawText(int x, int y, const std::string& text, Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12));

    /**
     * @brief Draws UTF-8 text clipped to a rectangle.
     * @param x Starting horizontal coordinate.
     * @param y Row coordinate.
     * @param text UTF-8 text.
     * @param clip Clipping rectangle.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawTextClipped(int x, int y, const std::string& text, const Rect& clip, Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12));

    /**
     * @brief Draws one aligned line of UTF-8 text inside a rectangle.
     * @param area Alignment and clipping rectangle.
     * @param text UTF-8 text.
     * @param alignment Horizontal alignment.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawTextAligned(const Rect& area, const std::string& text, TextAlignment alignment, Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12));

    /**
     * @brief Draws word-wrapped UTF-8 text inside a rectangle.
     * @param area Wrapping and clipping rectangle.
     * @param text UTF-8 text. Newlines create explicit line breaks.
     * @param alignment Horizontal alignment applied to each line.
     * @param foreground Foreground colour.
     * @param background Background colour.
     * @return Number of visible lines drawn.
     */
    static int drawWrappedText(const Rect& area, const std::string& text, TextAlignment alignment = TextAlignment::Left, Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12));

    /**
     * @brief Draws a horizontal line of repeated cells.
     * @param x Starting horizontal coordinate.
     * @param y Row coordinate.
     * @param width Number of cells to draw.
     * @param character Unicode code point repeated along the line.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawHorizontalLine(int x, int y, int width, char32_t character = U'─', Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12));

    /**
     * @brief Draws a vertical line of repeated cells.
     * @param x Column coordinate.
     * @param y Starting vertical coordinate.
     * @param height Number of cells to draw.
     * @param character Unicode code point repeated along the line.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawVerticalLine(int x, int y, int height, char32_t character = U'│', Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12));

    /**
     * @brief Fills the visible part of a rectangle with one cell value.
     * @param area Rectangle to fill.
     * @param character Unicode code point written into every visible cell.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void fillRect(const Rect& area, char32_t character = U' ', Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12));

    /**
     * @brief Draws a border around a rectangle.
     * @param area Border rectangle.
     * @param foreground Border colour.
     * @param background Cell background colour.
     * @param style Border character set.
     */
    static void drawBox(const Rect& area, Colour foreground = Colour(255, 255, 255), Colour background = Colour(12, 12, 12), BoxStyle style = BoxStyle::Single);

    /**
     * @brief Draws a filled, bordered panel with an optional title.
     * @param area Panel rectangle.
     * @param title UTF-8 title drawn in the top border.
     * @param border Border and title colour.
     * @param foreground Foreground colour used for the panel fill.
     * @param background Panel background colour.
     * @param style Border style.
     */
    static void drawPanel(const Rect& area, const std::string& title, Colour border = Colour(180, 180, 180), Colour foreground = Colour(255, 255, 255), Colour background = Colour(20, 20, 20), BoxStyle style = BoxStyle::Single);

    /** @brief Presents the current framebuffer to the terminal. */
    static void endFrame();
};

} // namespace tt
