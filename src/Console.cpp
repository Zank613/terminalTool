/**
 * @file Console.cpp
 * @brief Implements the tt::Console framebuffer renderer.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/Console.h"

#include "terminalTool/TerminalError.h"
#include "terminalTool/TerminalSession.h"
#include "detail/Unicode.h"
#include "platform/PlatformTerminal.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace {

[[nodiscard]] int clampToInt(const std::int64_t value) noexcept {
    return static_cast<int>(std::clamp<std::int64_t>(
        value,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()
    ));
}

[[nodiscard]] bool fitsInt(const std::int64_t value) noexcept {
    return value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max();
}

} // namespace

namespace tt {

int Console::frameWidth = 0;
int Console::frameHeight = 0;
bool Console::frameBufferActive = false;
bool Console::firstFrame = true;
std::vector<Console::Cell> Console::frameBuffer;
std::vector<Console::Cell> Console::previousBuffer;
std::vector<Console::Rect> Console::clipStack;

bool Console::Rect::contains(const int pointX, const int pointY) const {
    if (width <= 0 || height <= 0) {
        return false;
    }

    const std::int64_t left = x;
    const std::int64_t top = y;
    const std::int64_t right = left + static_cast<std::int64_t>(width);
    const std::int64_t bottom = top + static_cast<std::int64_t>(height);
    return
        static_cast<std::int64_t>(pointX) >= left &&
        static_cast<std::int64_t>(pointY) >= top &&
        static_cast<std::int64_t>(pointX) < right &&
        static_cast<std::int64_t>(pointY) < bottom;
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
    clipStack.clear();
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
    clipStack.clear();
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
    clipStack.clear();
}

Console::Size Console::terminalSize() {
    if (!TerminalSession::hasActiveSession() || !detail::platformIsInitialized()) {
        throw TerminalError(
            TerminalErrorCode::NoActiveSession,
            "tt::Console::terminalSize() requires an active TerminalSession."
        );
    }
    const detail::TerminalDimensions size = detail::platformTerminalSize();
    return Size { size.width, size.height };
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

Console::ScopedClip::ScopedClip(const Rect& area)
    : active(true) {
    Console::pushClip(area);
}

Console::ScopedClip::~ScopedClip() {
    if (active) {
        Console::popClip();
    }
}


void Console::pushClip(const Rect& area) {
    const Rect base = activeClip();
    clipStack.push_back(intersect(base, area));
}

void Console::popClip() noexcept {
#ifndef NDEBUG
    assert(!clipStack.empty() && "tt::Console::popClip() called with an empty clip stack");
#endif
    if (!clipStack.empty()) {
        clipStack.pop_back();
    }
}

void Console::clearClips() noexcept {
    clipStack.clear();
}

Console::Rect Console::currentClip() noexcept {
    return activeClip();
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
    if (!frameBufferActive || !isInsideFrame(x, y) || !activeClip().contains(x, y)) {
        return;
    }

    const std::size_t index =
        static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth) +
        static_cast<std::size_t>(x);

    Cell& cell = frameBuffer[index];
    cell.character = detail::sanitizeTerminalCell(character);
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
    drawTextClipped(x, y, text, activeClip(), foreground, background);
}

void Console::drawTextClipped(
    const int x,
    const int y,
    const std::string& text,
    const Rect& clip,
    const Colour foreground,
    const Colour background
) {
    if (!frameBufferActive) {
        return;
    }

    const Rect visibleClip = intersect(intersect(clip, frameRect()), activeClip());

    if (visibleClip.width <= 0 || visibleClip.height <= 0 ||
        static_cast<std::int64_t>(y) < static_cast<std::int64_t>(visibleClip.y) ||
        static_cast<std::int64_t>(y) >= static_cast<std::int64_t>(visibleClip.y) + visibleClip.height) {
        return;
    }

    const std::vector<char32_t> characters = decodeUtf8(text);

    for (std::size_t i = 0; i < characters.size(); i++) {
        const std::int64_t cellX = static_cast<std::int64_t>(x) + static_cast<std::int64_t>(i);
        if (cellX < std::numeric_limits<int>::min() || cellX > std::numeric_limits<int>::max()) {
            continue;
        }
        const int safeCellX = static_cast<int>(cellX);
        if (visibleClip.contains(safeCellX, y)) {
            drawCell(safeCellX, y, characters[i], foreground, background);
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

    const std::int64_t textWidth = static_cast<std::int64_t>(decodeUtf8(text).size());
    std::int64_t alignedX = area.x;

    switch (alignment) {
        case TextAlignment::Left:
            break;

        case TextAlignment::Centre:
            alignedX = static_cast<std::int64_t>(area.x) +
                (static_cast<std::int64_t>(area.width) - textWidth) / 2;
            break;

        case TextAlignment::Right:
            alignedX = static_cast<std::int64_t>(area.x) + area.width - textWidth;
            break;
    }

    drawTextClipped(clampToInt(alignedX), area.y, text, area, foreground, background);
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

        std::int64_t alignedX = area.x;

        switch (alignment) {
            case TextAlignment::Left:
                break;

            case TextAlignment::Centre:
                alignedX = static_cast<std::int64_t>(area.x) +
                    (static_cast<std::int64_t>(area.width) - static_cast<std::int64_t>(line.size())) / 2;
                break;

            case TextAlignment::Right:
                alignedX = static_cast<std::int64_t>(area.x) + area.width -
                    static_cast<std::int64_t>(line.size());
                break;
        }

        const std::int64_t lineY = static_cast<std::int64_t>(area.y) + lineIndex;
        if (fitsInt(lineY)) {
            drawTextClipped(clampToInt(alignedX), static_cast<int>(lineY), encodedLine, area, foreground, background);
        }
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

    const Rect visible = intersect(intersect(Rect { x, y, width, 1 }, frameRect()), activeClip());
    for (int cellX = visible.x; cellX < visible.x + visible.width; cellX++) {
        drawCell(cellX, y, character, foreground, background);
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

    const Rect visible = intersect(intersect(Rect { x, y, 1, height }, frameRect()), activeClip());
    for (int cellY = visible.y; cellY < visible.y + visible.height; cellY++) {
        drawCell(x, cellY, character, foreground, background);
    }
}

void Console::fillRect(
    const Rect& area,
    const char32_t character,
    const Colour foreground,
    const Colour background
) {
    const Rect visibleArea = intersect(intersect(area, frameRect()), activeClip());

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
    const std::int64_t left = area.x;
    const std::int64_t top = area.y;
    const std::int64_t right = left + static_cast<std::int64_t>(area.width) - 1;
    const std::int64_t bottom = top + static_cast<std::int64_t>(area.height) - 1;

    if (fitsInt(left + 1) && fitsInt(top)) {
        drawHorizontalLine(clampToInt(left + 1), static_cast<int>(top), area.width - 2, characters.horizontal, foreground, background);
    }
    if (fitsInt(left + 1) && fitsInt(bottom)) {
        drawHorizontalLine(clampToInt(left + 1), static_cast<int>(bottom), area.width - 2, characters.horizontal, foreground, background);
    }
    if (fitsInt(left) && fitsInt(top + 1)) {
        drawVerticalLine(static_cast<int>(left), clampToInt(top + 1), area.height - 2, characters.vertical, foreground, background);
    }
    if (fitsInt(right) && fitsInt(top + 1)) {
        drawVerticalLine(static_cast<int>(right), clampToInt(top + 1), area.height - 2, characters.vertical, foreground, background);
    }

    if (fitsInt(left) && fitsInt(top)) drawCell(static_cast<int>(left), static_cast<int>(top), characters.topLeft, foreground, background);
    if (fitsInt(right) && fitsInt(top)) drawCell(static_cast<int>(right), static_cast<int>(top), characters.topRight, foreground, background);
    if (fitsInt(left) && fitsInt(bottom)) drawCell(static_cast<int>(left), static_cast<int>(bottom), characters.bottomLeft, foreground, background);
    if (fitsInt(right) && fitsInt(bottom)) drawCell(static_cast<int>(right), static_cast<int>(bottom), characters.bottomRight, foreground, background);
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
        const std::int64_t titleX = static_cast<std::int64_t>(area.x) + 2;
        if (fitsInt(titleX)) {
            const Rect titleArea { static_cast<int>(titleX), area.y, area.width - 4, 1 };
            drawTextClipped(titleArea.x, titleArea.y, " " + title + " ", titleArea, border, background);
        }
    }
}

void Console::endFrame() {
    if (!TerminalSession::hasActiveSession() || !detail::platformIsInitialized()) {
        throw TerminalError(
            TerminalErrorCode::NoActiveSession,
            "tt::Console::endFrame() requires an active TerminalSession."
        );
    }
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
    if (!output.empty()) {
        detail::platformWriteOutput(output);
    }
}

bool Console::isInsideFrame(const int x, const int y) {
    return x >= 0 && y >= 0 && x < frameWidth && y < frameHeight;
}

Console::Rect Console::frameRect() {
    return Rect { 0, 0, frameWidth, frameHeight };
}

Console::Rect Console::activeClip() {
    return clipStack.empty() ? frameRect() : clipStack.back();
}

Console::Rect Console::intersect(const Rect& first, const Rect& second) {
    const std::int64_t firstRight = static_cast<std::int64_t>(first.x) + std::max(0, first.width);
    const std::int64_t firstBottom = static_cast<std::int64_t>(first.y) + std::max(0, first.height);
    const std::int64_t secondRight = static_cast<std::int64_t>(second.x) + std::max(0, second.width);
    const std::int64_t secondBottom = static_cast<std::int64_t>(second.y) + std::max(0, second.height);

    const std::int64_t left = std::max<std::int64_t>(first.x, second.x);
    const std::int64_t top = std::max<std::int64_t>(first.y, second.y);
    const std::int64_t right = std::min(firstRight, secondRight);
    const std::int64_t bottom = std::min(firstBottom, secondBottom);

    if (right <= left || bottom <= top) {
        const int safeLeft = static_cast<int>(std::clamp<std::int64_t>(
            left, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
        const int safeTop = static_cast<int>(std::clamp<std::int64_t>(
            top, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
        return Rect { safeLeft, safeTop, 0, 0 };
    }

    const int safeLeft = static_cast<int>(std::clamp<std::int64_t>(
        left, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
    const int safeTop = static_cast<int>(std::clamp<std::int64_t>(
        top, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
    const int safeWidth = static_cast<int>(std::min<std::int64_t>(
        right - left, std::numeric_limits<int>::max()));
    const int safeHeight = static_cast<int>(std::min<std::int64_t>(
        bottom - top, std::numeric_limits<int>::max()));
    return Rect { safeLeft, safeTop, safeWidth, safeHeight };
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
    return detail::encodeUtf8(character);
}

std::vector<char32_t> Console::decodeUtf8(const std::string& text) {
    return detail::decodeUtf8(text);
}

} // namespace tt
