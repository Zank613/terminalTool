/**
 * @file Console.cpp
 * @brief Implements the tt::Console framebuffer renderer.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/Console.h"

#include "terminalTool/TerminalError.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace tt {

int Console::frameWidth = 0;
int Console::frameHeight = 0;
bool Console::frameBufferActive = false;
bool Console::firstFrame = true;
std::vector<Console::Cell> Console::frameBuffer;
std::vector<Console::Cell> Console::previousBuffer;

bool Console::Rect::contains(const int pointX, const int pointY) const {
    return
        pointX >= x &&
        pointY >= y &&
        pointX < x + width &&
        pointY < y + height;
}

bool Console::Cell::operator==(const Cell& other) const {
    return
        character == other.character &&
        foreground == other.foreground &&
        background == other.background;
}

bool Console::Cell::operator!=(const Cell& other) const {
    return !(*this == other);
}

std::size_t Console::checkedCellCount(const int width, const int height, const bool resizing) {
    const TerminalErrorCode code = resizing
        ? TerminalErrorCode::FrameBufferResizeFailed
        : TerminalErrorCode::FrameBufferInitializationFailed;

    if (width <= 0 || height <= 0) {
        throw TerminalError(code, "terminalTool received invalid framebuffer dimensions.");
    }

    const std::size_t unsignedWidth = static_cast<std::size_t>(width);
    const std::size_t unsignedHeight = static_cast<std::size_t>(height);

    if (unsignedWidth > std::numeric_limits<std::size_t>::max() / unsignedHeight) {
        throw TerminalError(code, "terminalTool framebuffer dimensions overflow the addressable cell count.");
    }

    return unsignedWidth * unsignedHeight;
}

void Console::initializeFrameBuffer(const int width, const int height) {
    const std::size_t cellCount = checkedCellCount(width, height, false);

    try {
        std::vector<Cell> newFrameBuffer(cellCount);
        std::vector<Cell> newPreviousBuffer(cellCount);

        frameBuffer.swap(newFrameBuffer);
        previousBuffer.swap(newPreviousBuffer);
    } catch (const std::bad_alloc&) {
        throw TerminalError(
            TerminalErrorCode::FrameBufferInitializationFailed,
            "terminalTool could not allocate its terminal framebuffer."
        );
    } catch (const std::length_error&) {
        throw TerminalError(
            TerminalErrorCode::FrameBufferInitializationFailed,
            "terminalTool framebuffer dimensions exceed the vector size limit."
        );
    }

    frameWidth = width;
    frameHeight = height;
    frameBufferActive = true;
    firstFrame = true;
}

void Console::resizeFrameBuffer(const int width, const int height) {
    if (!frameBufferActive) {
        initializeFrameBuffer(width, height);
        return;
    }

    if (width == frameWidth && height == frameHeight) {
        return;
    }

    const std::size_t cellCount = checkedCellCount(width, height, true);

    try {
        std::vector<Cell> newFrameBuffer(cellCount);
        std::vector<Cell> newPreviousBuffer(cellCount);

        frameBuffer.swap(newFrameBuffer);
        previousBuffer.swap(newPreviousBuffer);
    } catch (const std::bad_alloc&) {
        throw TerminalError(
            TerminalErrorCode::FrameBufferResizeFailed,
            "terminalTool could not allocate the resized terminal framebuffer."
        );
    } catch (const std::length_error&) {
        throw TerminalError(
            TerminalErrorCode::FrameBufferResizeFailed,
            "terminalTool resized framebuffer dimensions exceed the vector size limit."
        );
    }

    frameWidth = width;
    frameHeight = height;
    firstFrame = true;
}

bool Console::resizeToTerminal() {
    const Size size = terminalSize();

    if (size.width == frameWidth && size.height == frameHeight) {
        return false;
    }

    resizeFrameBuffer(size.width, size.height);
    return true;
}

void Console::shutdownFrameBuffer() noexcept {
    frameBuffer.clear();
    previousBuffer.clear();
    frameWidth = 0;
    frameHeight = 0;
    firstFrame = true;
    frameBufferActive = false;
}

Console::Size Console::terminalSize() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info {};
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

    if (output == INVALID_HANDLE_VALUE || output == nullptr) {
        throw TerminalError(
            TerminalErrorCode::InvalidOutputHandle,
            "terminalTool could not obtain the standard output handle while querying terminal size.",
            static_cast<std::uint32_t>(GetLastError())
        );
    }

    if (!GetConsoleScreenBufferInfo(output, &info)) {
        throw TerminalError(
            TerminalErrorCode::QueryTerminalSizeFailed,
            "terminalTool could not query the visible Windows console size.",
            static_cast<std::uint32_t>(GetLastError())
        );
    }

    return Size {
        static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1),
        static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1)
    };
#else
    winsize size {};

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_col == 0 || size.ws_row == 0) {
        throw TerminalError(
            TerminalErrorCode::QueryTerminalSizeFailed,
            "terminalTool could not query the visible terminal size.",
            static_cast<std::uint32_t>(errno)
        );
    }

    return Size {
        static_cast<int>(size.ws_col),
        static_cast<int>(size.ws_row)
    };
#endif
}

int Console::getFrameWidth() noexcept {
    return frameWidth;
}

int Console::getFrameHeight() noexcept {
    return frameHeight;
}

bool Console::isActive() noexcept {
    return frameBufferActive;
}

void Console::invalidate() noexcept {
    if (frameBufferActive) {
        firstFrame = true;
    }
}

void Console::beginFrame(const Colour foreground, const Colour background) {
    if (!frameBufferActive) {
        return;
    }

    const Cell emptyCell { U' ', foreground, background };
    std::fill(frameBuffer.begin(), frameBuffer.end(), emptyCell);
}

void Console::drawCell(
    const int x,
    const int y,
    const char32_t character,
    const Colour foreground,
    const Colour background
) {
    if (!frameBufferActive || !isInsideFrame(x, y)) {
        return;
    }

    const std::size_t index =
        static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth) +
        static_cast<std::size_t>(x);

    Cell& cell = frameBuffer[index];
    cell.character = character;
    cell.foreground = foreground;
    cell.background = background;
}

void Console::drawText(
    const int x,
    const int y,
    const std::string& text,
    const Colour foreground,
    const Colour background
) {
    drawTextClipped(x, y, text, frameRect(), foreground, background);
}

void Console::drawTextClipped(
    const int x,
    const int y,
    const std::string& text,
    const Rect& clip,
    const Colour foreground,
    const Colour background
) {
    if (!frameBufferActive || y < clip.y || y >= clip.y + clip.height) {
        return;
    }

    const Rect visibleClip = intersect(clip, frameRect());

    if (visibleClip.width <= 0 || visibleClip.height <= 0) {
        return;
    }

    const std::vector<char32_t> characters = decodeUtf8(text);

    for (std::size_t i = 0; i < characters.size(); i++) {
        const int cellX = x + static_cast<int>(i);

        if (visibleClip.contains(cellX, y)) {
            drawCell(cellX, y, characters[i], foreground, background);
        }
    }
}

void Console::drawTextAligned(
    const Rect& area,
    const std::string& text,
    const TextAlignment alignment,
    const Colour foreground,
    const Colour background
) {
    if (area.width <= 0 || area.height <= 0) {
        return;
    }

    const int textWidth = static_cast<int>(decodeUtf8(text).size());
    int x = area.x;

    switch (alignment) {
        case TextAlignment::Left:
            break;

        case TextAlignment::Centre:
            x = area.x + (area.width - textWidth) / 2;
            break;

        case TextAlignment::Right:
            x = area.x + area.width - textWidth;
            break;
    }

    drawTextClipped(x, area.y, text, area, foreground, background);
}

int Console::drawWrappedText(
    const Rect& area,
    const std::string& text,
    const TextAlignment alignment,
    const Colour foreground,
    const Colour background
) {
    if (!frameBufferActive || area.width <= 0 || area.height <= 0) {
        return 0;
    }

    const std::vector<char32_t> characters = decodeUtf8(text);
    std::vector<std::vector<char32_t>> lines;
    std::vector<char32_t> currentLine;
    std::vector<char32_t> currentWord;

    const auto flushLine = [&lines, &currentLine](const bool includeEmpty) {
        if (!currentLine.empty() || includeEmpty) {
            lines.push_back(currentLine);
            currentLine.clear();
        }
    };

    const auto appendWord = [&lines, &currentLine, &currentWord, &flushLine, &area]() {
        if (currentWord.empty()) {
            return;
        }

        while (static_cast<int>(currentWord.size()) > area.width) {
            if (!currentLine.empty()) {
                flushLine(false);
            }

            lines.emplace_back(currentWord.begin(), currentWord.begin() + area.width);
            currentWord.erase(currentWord.begin(), currentWord.begin() + area.width);
        }

        if (currentWord.empty()) {
            return;
        }

        if (currentLine.empty()) {
            currentLine = currentWord;
        } else if (static_cast<int>(currentLine.size() + 1 + currentWord.size()) <= area.width) {
            currentLine.push_back(U' ');
            currentLine.insert(currentLine.end(), currentWord.begin(), currentWord.end());
        } else {
            flushLine(false);
            currentLine = currentWord;
        }

        currentWord.clear();
    };

    bool endedWithNewLine = false;

    for (const char32_t character : characters) {
        if (character == U'\r') {
            continue;
        }

        if (character == U'\n') {
            appendWord();
            flushLine(true);
            endedWithNewLine = true;
            continue;
        }

        endedWithNewLine = false;

        if (character == U' ' || character == U'\t') {
            appendWord();
            continue;
        }

        currentWord.push_back(character);
    }

    appendWord();

    if (!currentLine.empty() || lines.empty() || endedWithNewLine) {
        flushLine(true);
    }

    const int visibleLines = std::min(area.height, static_cast<int>(lines.size()));

    for (int lineIndex = 0; lineIndex < visibleLines; lineIndex++) {
        const std::vector<char32_t>& line = lines[static_cast<std::size_t>(lineIndex)];
        std::string encodedLine;

        for (const char32_t character : line) {
            encodedLine += encodeUtf8(character);
        }

        int x = area.x;

        switch (alignment) {
            case TextAlignment::Left:
                break;

            case TextAlignment::Centre:
                x = area.x + (area.width - static_cast<int>(line.size())) / 2;
                break;

            case TextAlignment::Right:
                x = area.x + area.width - static_cast<int>(line.size());
                break;
        }

        drawTextClipped(x, area.y + lineIndex, encodedLine, area, foreground, background);
    }

    return visibleLines;
}

void Console::drawHorizontalLine(
    const int x,
    const int y,
    const int width,
    const char32_t character,
    const Colour foreground,
    const Colour background
) {
    if (width <= 0) {
        return;
    }

    for (int offset = 0; offset < width; offset++) {
        drawCell(x + offset, y, character, foreground, background);
    }
}

void Console::drawVerticalLine(
    const int x,
    const int y,
    const int height,
    const char32_t character,
    const Colour foreground,
    const Colour background
) {
    if (height <= 0) {
        return;
    }

    for (int offset = 0; offset < height; offset++) {
        drawCell(x, y + offset, character, foreground, background);
    }
}

void Console::fillRect(
    const Rect& area,
    const char32_t character,
    const Colour foreground,
    const Colour background
) {
    const Rect visibleArea = intersect(area, frameRect());

    if (visibleArea.width <= 0 || visibleArea.height <= 0) {
        return;
    }

    for (int y = visibleArea.y; y < visibleArea.y + visibleArea.height; y++) {
        for (int x = visibleArea.x; x < visibleArea.x + visibleArea.width; x++) {
            drawCell(x, y, character, foreground, background);
        }
    }
}

void Console::drawBox(
    const Rect& area,
    const Colour foreground,
    const Colour background,
    const BoxStyle style
) {
    if (area.width < 2 || area.height < 2) {
        return;
    }

    const BoxCharacters characters = boxCharacters(style);

    drawHorizontalLine(area.x + 1, area.y, area.width - 2, characters.horizontal, foreground, background);
    drawHorizontalLine(area.x + 1, area.y + area.height - 1, area.width - 2, characters.horizontal, foreground, background);
    drawVerticalLine(area.x, area.y + 1, area.height - 2, characters.vertical, foreground, background);
    drawVerticalLine(area.x + area.width - 1, area.y + 1, area.height - 2, characters.vertical, foreground, background);

    drawCell(area.x, area.y, characters.topLeft, foreground, background);
    drawCell(area.x + area.width - 1, area.y, characters.topRight, foreground, background);
    drawCell(area.x, area.y + area.height - 1, characters.bottomLeft, foreground, background);
    drawCell(area.x + area.width - 1, area.y + area.height - 1, characters.bottomRight, foreground, background);
}

void Console::drawPanel(
    const Rect& area,
    const std::string& title,
    const Colour border,
    const Colour foreground,
    const Colour background,
    const BoxStyle style
) {
    if (area.width < 2 || area.height < 2) {
        return;
    }

    fillRect(area, U' ', foreground, background);
    drawBox(area, border, background, style);

    if (!title.empty() && area.width > 4) {
        const Rect titleArea { area.x + 2, area.y, area.width - 4, 1 };
        drawTextClipped(titleArea.x, titleArea.y, " " + title + " ", titleArea, border, background);
    }
}

void Console::endFrame() {
    if (!frameBufferActive) {
        return;
    }

    try {
        if (firstFrame) {
            renderFullFrame();
            previousBuffer.swap(frameBuffer);
            firstFrame = false;
            return;
        }

        std::size_t changedCells = 0;

        for (std::size_t i = 0; i < frameBuffer.size(); i++) {
            if (frameBuffer[i] != previousBuffer[i]) {
                changedCells++;
            }
        }

        if (changedCells == 0) {
            previousBuffer.swap(frameBuffer);
            return;
        }

        if (changedCells * 2 > frameBuffer.size()) {
            renderFullFrame();
        } else {
            renderDifferentialFrame();
        }

        previousBuffer.swap(frameBuffer);
    } catch (...) {
        firstFrame = true;
        throw;
    }
}

void Console::renderFullFrame() {
    std::string output;
    output.reserve(frameBuffer.size() * 5);

    Colour currentForeground = Colours::DefaultForeground;
    Colour currentBackground = Colours::DefaultBackground;
    bool colourInitialized = false;

    for (int y = 0; y < frameHeight; y++) {
        output += cursorPosition(0, y);

        for (int x = 0; x < frameWidth; x++) {
            const std::size_t index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth) +
                static_cast<std::size_t>(x);
            const Cell& cell = frameBuffer[index];

            if (!colourInitialized || cell.foreground != currentForeground) {
                output += cell.foreground.foreground();
                currentForeground = cell.foreground;
            }

            if (!colourInitialized || cell.background != currentBackground) {
                output += cell.background.background();
                currentBackground = cell.background;
            }

            colourInitialized = true;
            output += encodeUtf8(cell.character);
        }
    }

    output += Colour::RESET;
    writeOutput(output);
}

void Console::renderDifferentialFrame() {
    std::string output;
    Colour currentForeground = Colours::DefaultForeground;
    Colour currentBackground = Colours::DefaultBackground;
    bool colourInitialized = false;

    for (int y = 0; y < frameHeight; y++) {
        int x = 0;

        while (x < frameWidth) {
            const std::size_t index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth) +
                static_cast<std::size_t>(x);

            if (frameBuffer[index] == previousBuffer[index]) {
                x++;
                continue;
            }

            output += cursorPosition(x, y);

            while (x < frameWidth) {
                const std::size_t runIndex =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth) +
                    static_cast<std::size_t>(x);
                const Cell& currentCell = frameBuffer[runIndex];
                const Cell& previousCell = previousBuffer[runIndex];

                if (currentCell == previousCell) {
                    break;
                }

                if (!colourInitialized || currentCell.foreground != currentForeground) {
                    output += currentCell.foreground.foreground();
                    currentForeground = currentCell.foreground;
                }

                if (!colourInitialized || currentCell.background != currentBackground) {
                    output += currentCell.background.background();
                    currentBackground = currentCell.background;
                }

                colourInitialized = true;
                output += encodeUtf8(currentCell.character);
                x++;
            }
        }
    }

    if (!output.empty()) {
        output += Colour::RESET;
        writeOutput(output);
    }
}

void Console::writeOutput(const std::string& output) {
    if (output.empty()) {
        return;
    }

    errno = 0;
    std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
    std::cout.flush();

    if (!std::cout.good()) {
        throw TerminalError(
            TerminalErrorCode::WriteOutputFailed,
            "terminalTool could not write or flush terminal output.",
            static_cast<std::uint32_t>(errno)
        );
    }
}

bool Console::isInsideFrame(const int x, const int y) {
    return x >= 0 && y >= 0 && x < frameWidth && y < frameHeight;
}

Console::Rect Console::frameRect() {
    return Rect { 0, 0, frameWidth, frameHeight };
}

Console::Rect Console::intersect(const Rect& first, const Rect& second) {
    const int left = std::max(first.x, second.x);
    const int top = std::max(first.y, second.y);
    const int right = std::min(first.x + first.width, second.x + second.width);
    const int bottom = std::min(first.y + first.height, second.y + second.height);

    return Rect { left, top, std::max(0, right - left), std::max(0, bottom - top) };
}

Console::BoxCharacters Console::boxCharacters(const BoxStyle style) {
    switch (style) {
        case BoxStyle::Double:
            return BoxCharacters { U'═', U'║', U'╔', U'╗', U'╚', U'╝' };

        case BoxStyle::Ascii:
            return BoxCharacters { U'-', U'|', U'+', U'+', U'+', U'+' };

        case BoxStyle::Single:
        default:
            return BoxCharacters { U'─', U'│', U'┌', U'┐', U'└', U'┘' };
    }
}

std::string Console::cursorPosition(const int x, const int y) {
    return "\033[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
}

std::string Console::encodeUtf8(const char32_t character) {
    std::string result;

    if (character <= 0x7F) {
        result.push_back(static_cast<char>(character));
    } else if (character <= 0x7FF) {
        result.push_back(static_cast<char>(0xC0 | ((character >> 6) & 0x1F)));
        result.push_back(static_cast<char>(0x80 | (character & 0x3F)));
    } else if (character <= 0xFFFF) {
        result.push_back(static_cast<char>(0xE0 | ((character >> 12) & 0x0F)));
        result.push_back(static_cast<char>(0x80 | ((character >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (character & 0x3F)));
    } else if (character <= 0x10FFFF) {
        result.push_back(static_cast<char>(0xF0 | ((character >> 18) & 0x07)));
        result.push_back(static_cast<char>(0x80 | ((character >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((character >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (character & 0x3F)));
    } else {
        result = "?";
    }

    return result;
}

std::vector<char32_t> Console::decodeUtf8(const std::string& text) {
    std::vector<char32_t> result;
    result.reserve(text.size());

    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        char32_t character = U'?';
        std::size_t length = 1;

        if ((first & 0x80) == 0) {
            character = first;
        } else if ((first & 0xE0) == 0xC0 && i + 1 < text.size()) {
            character =
                static_cast<char32_t>(first & 0x1F) << 6 |
                static_cast<char32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F);
            length = 2;
        } else if ((first & 0xF0) == 0xE0 && i + 2 < text.size()) {
            character =
                static_cast<char32_t>(first & 0x0F) << 12 |
                static_cast<char32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6 |
                static_cast<char32_t>(static_cast<unsigned char>(text[i + 2]) & 0x3F);
            length = 3;
        } else if ((first & 0xF8) == 0xF0 && i + 3 < text.size()) {
            character =
                static_cast<char32_t>(first & 0x07) << 18 |
                static_cast<char32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12 |
                static_cast<char32_t>(static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6 |
                static_cast<char32_t>(static_cast<unsigned char>(text[i + 3]) & 0x3F);
            length = 4;
        }

        result.push_back(character);
        i += length;
    }

    return result;
}

} // namespace tt
