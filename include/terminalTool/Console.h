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

class TerminalSession;
namespace detail { class TestAccess; }

/**
 * @brief Static cell-based terminal framebuffer and drawing API.
 *
 * Call beginFrame(), issue drawing commands, and then call endFrame(). The
 * renderer compares the current framebuffer with the previously presented one
 * and chooses either a complete or differential terminal update.
 *
 * Framebuffer lifetime is owned exclusively by TerminalSession. Applications
 * cannot initialize, resize, or shut it down directly.
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

    /**
     * @brief RAII clipping region pushed onto Console's clip stack.
     *
     * Constructing the object intersects the requested rectangle with the
     * current clip. Destruction restores the previous clipping region.
     */
    class ScopedClip {
    private:
        bool active = false;

    public:
        /** @brief Pushes a clipping rectangle. */
        explicit ScopedClip(const Rect& area);

        /** @brief Pops the clipping rectangle when still active. */
        ~ScopedClip();

        ScopedClip(const ScopedClip&) = delete;
        ScopedClip& operator=(const ScopedClip&) = delete;

        ScopedClip(ScopedClip&&) = delete;
        ScopedClip& operator=(ScopedClip&&) = delete;
    };

private:
    friend class TerminalSession;
    friend class detail::TestAccess;

    struct Cell {
        char32_t character = U' ';
        Colour foreground = Colours::DefaultForeground;
        Colour background = Colours::DefaultBackground;

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
    static std::vector<Rect> clipStack;

    static void initializeFrameBuffer(int width, int height);
    static void resizeFrameBuffer(int width, int height);
    static bool resizeToTerminal();
    static void shutdownFrameBuffer() noexcept;

    static void renderFullFrame();
    static void renderDifferentialFrame();
    static void writeOutput(const std::string& output);

    [[nodiscard]] static std::size_t checkedCellCount(int width, int height, bool resizing);
    [[nodiscard]] static bool isInsideFrame(int x, int y);
    [[nodiscard]] static Rect frameRect();
    [[nodiscard]] static Rect activeClip();
    [[nodiscard]] static Rect intersect(const Rect& first, const Rect& second);
    [[nodiscard]] static BoxCharacters boxCharacters(BoxStyle style);
    [[nodiscard]] static std::string cursorPosition(int x, int y);
    [[nodiscard]] static std::string encodeUtf8(char32_t character);
    [[nodiscard]] static std::vector<char32_t> decodeUtf8(const std::string& text);

public:
    /**
     * @return The current visible terminal size.
     * @throws TerminalError when the terminal dimensions cannot be queried.
     */
    [[nodiscard]] static Size terminalSize();

    /** @return Current framebuffer width in cells. */
    [[nodiscard]] static int getFrameWidth() noexcept;

    /** @return Current framebuffer height in cells. */
    [[nodiscard]] static int getFrameHeight() noexcept;

    /** @return `true` while a TerminalSession owns an initialized framebuffer. */
    [[nodiscard]] static bool isActive() noexcept;

    /**
     * @brief Forces the next endFrame() call to redraw the complete framebuffer.
     *
     * Call this after external code writes directly to the terminal or whenever
     * the physical terminal contents may no longer match terminalTool's stored
     * previous frame.
     */
    static void invalidate() noexcept;

    /**
     * @brief Pushes a clipping rectangle intersected with the current clip.
     *
     * All subsequent drawing operations are restricted until popClip() is
     * called. Prefer ScopedClip when lexical lifetime is available.
     */
    static void pushClip(const Rect& area);

    /** @brief Pops the latest clipping rectangle. Empty stacks are ignored. */
    static void popClip() noexcept;

    /** @brief Removes every user clipping rectangle. */
    static void clearClips() noexcept;

    /** @return The currently effective clipping rectangle. */
    [[nodiscard]] static Rect currentClip() noexcept;

    /**
     * @brief Clears the current framebuffer with one foreground/background pair.
     * @param foreground Default foreground colour for empty cells.
     * @param background Default background colour for empty cells.
     */
    static void beginFrame(Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground);

    /**
     * @brief Draws one Unicode code point into a framebuffer cell.
     * @param x Horizontal cell coordinate.
     * @param y Vertical cell coordinate.
     * @param character Unicode code point to draw.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawCell(int x, int y, char32_t character, Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground);

    /**
     * @brief Draws UTF-8 text clipped to the framebuffer.
     * @param x Starting horizontal coordinate.
     * @param y Row coordinate.
     * @param text UTF-8 text.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawText(int x, int y, const std::string& text, Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground);

    /**
     * @brief Draws UTF-8 text clipped to a rectangle.
     * @param x Starting horizontal coordinate.
     * @param y Row coordinate.
     * @param text UTF-8 text.
     * @param clip Clipping rectangle.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawTextClipped(int x, int y, const std::string& text, const Rect& clip, Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground);

    /**
     * @brief Draws one aligned line of UTF-8 text inside a rectangle.
     * @param area Alignment and clipping rectangle.
     * @param text UTF-8 text.
     * @param alignment Horizontal alignment.
     * @param foreground Foreground colour.
     * @param background Background colour.
     */
    static void drawTextAligned(const Rect& area, const std::string& text, TextAlignment alignment, Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground);

    /**
     * @brief Draws word-wrapped UTF-8 text inside a rectangle.
     * @param area Wrapping and clipping rectangle.
     * @param text UTF-8 text. Newlines create explicit line breaks.
     * @param alignment Horizontal alignment applied to each line.
     * @param foreground Foreground colour.
     * @param background Background colour.
     * @return Number of visible lines drawn.
     */
    static int drawWrappedText(const Rect& area, const std::string& text, TextAlignment alignment = TextAlignment::Left, Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground);

    /** @brief Draws a horizontal line of repeated cells. */
    static void drawHorizontalLine(int x, int y, int width, char32_t character = U'─', Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground);

    /** @brief Draws a vertical line of repeated cells. */
    static void drawVerticalLine(int x, int y, int height, char32_t character = U'│', Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground);

    /** @brief Fills the visible part of a rectangle with one cell value. */
    static void fillRect(const Rect& area, char32_t character = U' ', Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground);

    /** @brief Draws a border around a rectangle. */
    static void drawBox(const Rect& area, Colour foreground = Colours::DefaultForeground, Colour background = Colours::DefaultBackground, BoxStyle style = BoxStyle::Single);

    /** @brief Draws a filled, bordered panel with an optional title. */
    static void drawPanel(const Rect& area, const std::string& title, Colour border = Colour(180, 180, 180), Colour foreground = Colours::DefaultForeground, Colour background = Colour(20, 20, 20), BoxStyle style = BoxStyle::Single);

    /**
     * @brief Presents the current framebuffer to the terminal.
     * @throws TerminalError with TerminalErrorCode::WriteOutputFailed when the
     *         rendered frame cannot be written or flushed.
     */
    static void endFrame();
};

} // namespace tt
