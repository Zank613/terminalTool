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
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "terminalTool/Console.h"
#include "terminalTool/Input.h"
#undef private
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "terminalTool/Colour.h"
#include "terminalTool/TerminalError.h"
#include "terminalTool/Version.h"

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
    TT_CHECK(tt::Version::Patch == 1);
    TT_CHECK(std::string(tt::Version::String) == "0.2.1");
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
    tt::Console::shutdownFrameBuffer();
    tt::Console::initializeFrameBuffer(4, 3);

    TT_CHECK(tt::Console::isActive());
    TT_CHECK(tt::Console::getFrameWidth() == 4);
    TT_CHECK(tt::Console::getFrameHeight() == 3);
    TT_CHECK(tt::Console::frameBuffer.size() == 12);
    TT_CHECK(tt::Console::previousBuffer.size() == 12);

    tt::Console::beginFrame(tt::Colours::BrightWhite, tt::Colours::Black);
    tt::Console::drawCell(2, 1, U'@', tt::Colours::BrightCyan, tt::Colours::Black);

    const std::size_t index = 1U * 4U + 2U;
    TT_CHECK(tt::Console::frameBuffer[index].character == U'@');
    TT_CHECK(tt::Console::frameBuffer[index].foreground == tt::Colours::BrightCyan);

    tt::Console::firstFrame = false;
    tt::Console::invalidate();
    TT_CHECK(tt::Console::firstFrame);

    tt::Console::resizeFrameBuffer(7, 2);
    TT_CHECK(tt::Console::getFrameWidth() == 7);
    TT_CHECK(tt::Console::getFrameHeight() == 2);
    TT_CHECK(tt::Console::frameBuffer.size() == 14);
    TT_CHECK(tt::Console::previousBuffer.size() == 14);
    TT_CHECK(tt::Console::firstFrame);

    tt::Console::shutdownFrameBuffer();
    TT_CHECK(!tt::Console::isActive());
}

void testUtf8Helpers() {
    const std::vector<char32_t> decoded = tt::Console::decodeUtf8("Aç€😀");
    TT_CHECK(decoded.size() == 4);
    TT_CHECK(decoded[0] == U'A');
    TT_CHECK(decoded[1] == U'ç');
    TT_CHECK(decoded[2] == U'€');
    TT_CHECK(decoded[3] == U'😀');
    TT_CHECK(tt::Console::encodeUtf8(U'😀') == "😀");
}

void testInputStateAndText() {
    tt::Input::reset();
    tt::Input::setKeyState(tt::Key::A, true);
    TT_CHECK(tt::Input::isHeld(tt::Key::A));
    TT_CHECK(tt::Input::isPressed(tt::Key::A));

    tt::Input::pressed.fill(false);
    tt::Input::setKeyState(tt::Key::A, false);
    TT_CHECK(!tt::Input::isHeld(tt::Key::A));
    TT_CHECK(tt::Input::isReleased(tt::Key::A));

    tt::Input::reset();
    tt::Input::appendTextCodeUnit(0xD83D, 1);
    tt::Input::appendTextCodeUnit(0xDE00, 1);
    TT_CHECK(tt::Input::textInput() == "😀");
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
    testTerminalError();

    if (failures != 0) {
        std::cerr << failures << " terminalTool test(s) failed.\n";
        return 1;
    }

    std::cout << "All terminalTool tests passed.\n";
    return 0;
}
