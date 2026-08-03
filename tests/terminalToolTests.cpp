/**
 * @file terminalToolTests.cpp
 * @brief Core unit tests independent of an interactive terminal.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "detail/NativeInputEvent.h"
#include "terminalTool/Colour.h"
#include "terminalTool/Console.h"
#include "terminalTool/DeltaTime.h"
#include "terminalTool/Input.h"
#include "terminalTool/TerminalError.h"
#include "terminalTool/TerminalSession.h"
#include "terminalTool/Version.h"

namespace tt::detail {

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

    [[nodiscard]] static char32_t frameCharacter(const std::size_t index) {
        return Console::frameBuffer.at(index).character;
    }

    static void resetInput() noexcept {
        Input::reset();
    }

    static void processInput(const NativeInputEvent& event) {
        Input::processNativeEvent(event);
    }

    [[nodiscard]] static std::vector<char32_t> decodeUtf8(const std::string& text) {
        return Console::decodeUtf8(text);
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
    TT_CHECK(tt::Version::Minor == 3);
    TT_CHECK(tt::Version::Patch == 0);
    TT_CHECK(std::string(tt::Version::String) == "0.3.0");
}

void testColourCache() {
    const tt::Colour first(10, 20, 30);
    const tt::Colour second(10, 20, 30);
    TT_CHECK(first == second);
    TT_CHECK(&first.foreground() == &second.foreground());
    TT_CHECK(&first.background() == &second.background());
    TT_CHECK(tt::Colours::BrightWhite == tt::Colour(255, 255, 255));
}

void testModifierState() {
    tt::ModifierState modifiers;
    modifiers.leftShift = true;
    modifiers.rightControl = true;
    modifiers.rightAlt = true;
    modifiers.leftSuper = true;

    TT_CHECK(modifiers.shift());
    TT_CHECK(modifiers.control());
    TT_CHECK(modifiers.alt());
    TT_CHECK(modifiers.super());
    TT_CHECK(modifiers.altGr());
}

void testClipping() {
    tt::detail::TestAccess::shutdownFrameBuffer();
    tt::detail::TestAccess::initializeFrameBuffer(6, 5);
    tt::Console::beginFrame(tt::Colours::BrightWhite, tt::Colours::Black);

    tt::Console::pushClip({ 1, 1, 3, 2 });
    TT_CHECK(tt::Console::currentClip().contains(1, 1));
    TT_CHECK(!tt::Console::currentClip().contains(4, 1));

    tt::Console::drawCell(0, 0, U'X');
    tt::Console::drawCell(2, 1, U'A');

    {
        tt::Console::ScopedClip nested({ 2, 1, 1, 1 });
        tt::Console::drawCell(1, 1, U'B');
        tt::Console::drawCell(2, 1, U'C');
    }

    const std::size_t outside = 0;
    const std::size_t clipped = 1U * 6U + 1U;
    const std::size_t inside = 1U * 6U + 2U;
    TT_CHECK(tt::detail::TestAccess::frameCharacter(outside) == U' ');
    TT_CHECK(tt::detail::TestAccess::frameCharacter(clipped) == U' ');
    TT_CHECK(tt::detail::TestAccess::frameCharacter(inside) == U'C');

    tt::Console::popClip();
    TT_CHECK(tt::Console::currentClip().width == 6);
    TT_CHECK(tt::Console::currentClip().height == 5);

    tt::detail::TestAccess::shutdownFrameBuffer();
}

void testRawInputQueueAndState() {
    tt::detail::TestAccess::resetInput();

    tt::detail::NativeInputEvent down;
    down.type = tt::detail::NativeInputEventType::KeyDown;
    down.key = tt::Key::Oem1;
    down.repeatCount = 3;
    down.repeated = true;
    down.scanCode = 39;
    down.nativeKeyCode = 186;
    down.modifiers.leftShift = true;
    down.modifiers.capsLock = true;
    tt::detail::TestAccess::processInput(down);

    TT_CHECK(tt::Input::isHeld(tt::Key::Oem1));
    TT_CHECK(tt::Input::isPressed(tt::Key::Oem1));
    TT_CHECK(tt::Input::eventCount() == 1);

    const auto first = tt::Input::pollEvent();
    TT_CHECK(first.has_value());
    TT_CHECK(first->type == tt::InputEventType::KeyPressed);
    TT_CHECK(first->key.key == tt::Key::Oem1);
    TT_CHECK(first->key.repeated);
    TT_CHECK(first->key.repeatCount == 3);
    TT_CHECK(first->key.scanCode == 39);
    TT_CHECK(first->key.nativeKeyCode == 186);
    TT_CHECK(first->key.modifiers.shift());
    TT_CHECK(first->key.modifiers.capsLock);

    tt::detail::NativeInputEvent text;
    text.type = tt::detail::NativeInputEventType::Text;
    text.character = U'ş';
    tt::detail::TestAccess::processInput(text);
    TT_CHECK(tt::Input::textInput() == "ş");

    const auto textEvent = tt::Input::pollEvent();
    TT_CHECK(textEvent.has_value());
    TT_CHECK(textEvent->type == tt::InputEventType::TextEntered);
    TT_CHECK(textEvent->character == U'ş');

    tt::detail::NativeInputEvent focusLost;
    focusLost.type = tt::detail::NativeInputEventType::FocusLost;
    tt::detail::TestAccess::processInput(focusLost);
    TT_CHECK(!tt::Input::isFocused());
    TT_CHECK(tt::Input::focusLost());
    TT_CHECK(!tt::Input::isHeld(tt::Key::Oem1));
    TT_CHECK(tt::Input::isReleased(tt::Key::Oem1));

    tt::detail::NativeInputEvent focusGained;
    focusGained.type = tt::detail::NativeInputEventType::FocusGained;
    tt::detail::TestAccess::processInput(focusGained);
    TT_CHECK(tt::Input::isFocused());
    TT_CHECK(tt::Input::focusGained());

    tt::Input::clearEvents();
    TT_CHECK(!tt::Input::hasEvent());
}

void testUtf8AndDeltaTime() {
    const std::vector<char32_t> decoded = tt::detail::TestAccess::decodeUtf8("Aç€😀");
    TT_CHECK(decoded.size() == 4);
    TT_CHECK(decoded[3] == U'😀');

    tt::DeltaTime deltaTime;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    TT_CHECK(deltaTime.update() > 0.0);
    TT_CHECK(deltaTime.milliseconds() > 0.0);
    deltaTime.reset();
    TT_CHECK(deltaTime.seconds() == 0.0);
}

void testOptionsAndError() {
    tt::TerminalOptions options;
    options.title = "test";
    options.alternateScreen = false;
    options.hideCursor = false;
    options.enableFocusEvents = false;
    options.installSignalHandlers = false;

    TT_CHECK(options.title == "test");
    TT_CHECK(!options.alternateScreen);
    TT_CHECK(!options.hideCursor);
    TT_CHECK(!options.enableFocusEvents);
    TT_CHECK(!options.installSignalHandlers);

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
    testColourCache();
    testModifierState();
    testClipping();
    testRawInputQueueAndState();
    testUtf8AndDeltaTime();
    testOptionsAndError();

    if (failures != 0) {
        std::cerr << failures << " terminalTool core test(s) failed.\n";
        return 1;
    }

    std::cout << "All terminalTool core tests passed.\n";
    return 0;
}
