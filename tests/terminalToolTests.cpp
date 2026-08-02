/**
 * @file terminalToolTests.cpp
 * @brief Self-contained unit tests that do not require an interactive terminal.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "terminalTool/Colour.h"
#include "terminalTool/Console.h"
#include "terminalTool/DeltaTime.h"
#include "terminalTool/Input.h"
#include "terminalTool/TerminalError.h"
#include "terminalTool/Version.h"

namespace tt::detail {

/**
 * @brief Internal test-only access to private implementation state.
 *
 * Keeping the original access specifiers intact is important on MSVC because
 * access level is encoded into decorated symbol names.
 */
class TestAccess {
public:
    static void initializeFrameBuffer(const int width, const int height) {
        Console::initializeFrameBuffer(width, height);
    }

    static void resizeFrameBuffer(const int width, const int height) {
        Console::resizeFrameBuffer(width, height);
    }

    static void shutdownFrameBuffer() noexcept {
        Console::shutdownFrameBuffer();
    }

    [[nodiscard]] static std::size_t frameBufferSize() noexcept {
        return Console::frameBuffer.size();
    }

    [[nodiscard]] static std::size_t previousBufferSize() noexcept {
        return Console::previousBuffer.size();
    }

    [[nodiscard]] static char32_t frameCharacter(const std::size_t index) {
        return Console::frameBuffer.at(index).character;
    }

    [[nodiscard]] static Colour frameForeground(const std::size_t index) {
        return Console::frameBuffer.at(index).foreground;
    }

    static void setFirstFrame(const bool value) noexcept {
        Console::firstFrame = value;
    }

    [[nodiscard]] static bool isFirstFrame() noexcept {
        return Console::firstFrame;
    }

    [[nodiscard]] static std::string encodeUtf8(const char32_t character) {
        return Console::encodeUtf8(character);
    }

    [[nodiscard]] static std::vector<char32_t> decodeUtf8(const std::string& text) {
        return Console::decodeUtf8(text);
    }

    static void resetInput() noexcept {
        Input::reset();
    }

    static void setKeyState(const Key key, const bool isDown) {
        Input::setKeyState(key, isDown);
    }

    static void clearPressedKeys() noexcept {
        Input::pressed.fill(false);
    }

    static void appendTextCodeUnit(const char16_t codeUnit, const std::uint16_t repeatCount) {
        Input::appendTextCodeUnit(codeUnit, repeatCount);
    }
};

} // namespace tt::detail

namespace {

int failures = 0;

void check(const bool condition, const char* expression, const int line) {
    if (!condition) {
        std::cerr << "FAILED line " << line << ": " << expression << '\n';
        failures++;
    }
}

#define TT_CHECK(expression) check((expression), #expression, __LINE__)

void testVersion() {
    TT_CHECK(tt::Version::Major == 0);
    TT_CHECK(tt::Version::Minor == 2);
    TT_CHECK(tt::Version::Patch == 2);
    TT_CHECK(std::string(tt::Version::String) == "0.2.2");
}

void testColourCacheAndPalette() {
    const tt::Colour first(10, 20, 30);
    const tt::Colour second(10, 20, 30);

    TT_CHECK(first == second);
    TT_CHECK(&first.foreground() == &second.foreground());
    TT_CHECK(&first.background() == &second.background());
    TT_CHECK(first.foreground() == "\033[38;2;10;20;30m");
    TT_CHECK(first.background() == "\033[48;2;10;20;30m");
    TT_CHECK(tt::Colours::BrightWhite == tt::Colour(255, 255, 255));
}

void testRectangles() {
    const tt::Console::Rect rect { 3, 4, 5, 2 };
    TT_CHECK(rect.contains(3, 4));
    TT_CHECK(rect.contains(7, 5));
    TT_CHECK(!rect.contains(8, 5));
    TT_CHECK(!rect.contains(3, 6));
}

void testFramebufferLifecycleAndInvalidation() {
    tt::detail::TestAccess::shutdownFrameBuffer();
    tt::detail::TestAccess::initializeFrameBuffer(4, 3);

    TT_CHECK(tt::Console::isActive());
    TT_CHECK(tt::Console::getFrameWidth() == 4);
    TT_CHECK(tt::Console::getFrameHeight() == 3);
    TT_CHECK(tt::detail::TestAccess::frameBufferSize() == 12);
    TT_CHECK(tt::detail::TestAccess::previousBufferSize() == 12);

    tt::Console::beginFrame(tt::Colours::BrightWhite, tt::Colours::Black);
    tt::Console::drawCell(2, 1, U'@', tt::Colours::BrightCyan, tt::Colours::Black);

    const std::size_t index = 1U * 4U + 2U;
    TT_CHECK(tt::detail::TestAccess::frameCharacter(index) == U'@');
    TT_CHECK(tt::detail::TestAccess::frameForeground(index) == tt::Colours::BrightCyan);

    tt::detail::TestAccess::setFirstFrame(false);
    tt::Console::invalidate();
    TT_CHECK(tt::detail::TestAccess::isFirstFrame());

    tt::detail::TestAccess::resizeFrameBuffer(7, 2);
    TT_CHECK(tt::Console::getFrameWidth() == 7);
    TT_CHECK(tt::Console::getFrameHeight() == 2);
    TT_CHECK(tt::detail::TestAccess::frameBufferSize() == 14);
    TT_CHECK(tt::detail::TestAccess::previousBufferSize() == 14);
    TT_CHECK(tt::detail::TestAccess::isFirstFrame());

    tt::detail::TestAccess::shutdownFrameBuffer();
    TT_CHECK(!tt::Console::isActive());
}

void testUtf8Helpers() {
    const std::vector<char32_t> decoded = tt::detail::TestAccess::decodeUtf8("Aç€😀");
    TT_CHECK(decoded.size() == 4);
    TT_CHECK(decoded[0] == U'A');
    TT_CHECK(decoded[1] == U'ç');
    TT_CHECK(decoded[2] == U'€');
    TT_CHECK(decoded[3] == U'😀');
    TT_CHECK(tt::detail::TestAccess::encodeUtf8(U'😀') == "😀");
}

void testInputStateAndText() {
    tt::detail::TestAccess::resetInput();
    tt::detail::TestAccess::setKeyState(tt::Key::A, true);
    TT_CHECK(tt::Input::isHeld(tt::Key::A));
    TT_CHECK(tt::Input::isPressed(tt::Key::A));

    tt::detail::TestAccess::clearPressedKeys();
    tt::detail::TestAccess::setKeyState(tt::Key::A, false);
    TT_CHECK(!tt::Input::isHeld(tt::Key::A));
    TT_CHECK(tt::Input::isReleased(tt::Key::A));

    tt::detail::TestAccess::resetInput();
    tt::detail::TestAccess::appendTextCodeUnit(0xD83D, 1);
    tt::detail::TestAccess::appendTextCodeUnit(0xDE00, 1);
    TT_CHECK(tt::Input::textInput() == "😀");
}

void testDeltaTime() {
    tt::DeltaTime deltaTime;

    TT_CHECK(deltaTime.seconds() == 0.0);
    TT_CHECK(deltaTime.milliseconds() == 0.0);

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const double measuredSeconds = deltaTime.update();

    TT_CHECK(measuredSeconds > 0.0);
    TT_CHECK(deltaTime.seconds() == measuredSeconds);
    TT_CHECK(deltaTime.milliseconds() > 0.0);

    deltaTime.reset();
    TT_CHECK(deltaTime.seconds() == 0.0);
    TT_CHECK(deltaTime.milliseconds() == 0.0);
}

void testTerminalError() {
    const tt::TerminalError error(
        tt::TerminalErrorCode::ReadInputFailed,
        "read failed",
        42
    );

    TT_CHECK(error.code() == tt::TerminalErrorCode::ReadInputFailed);
    TT_CHECK(error.nativeErrorCode() == 42);
    TT_CHECK(std::string(error.what()) == "read failed");
}

} // namespace

int main() {
    testVersion();
    testColourCacheAndPalette();
    testRectangles();
    testFramebufferLifecycleAndInvalidation();
    testUtf8Helpers();
    testInputStateAndText();
    testDeltaTime();
    testTerminalError();

    if (failures != 0) {
        std::cerr << failures << " terminalTool test(s) failed.\n";
        return 1;
    }

    std::cout << "All terminalTool tests passed.\n";
    return 0;
}
