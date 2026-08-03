/**
 * @file escapeSequenceParserTests.cpp
 * @brief Tests the shared incremental POSIX input parser.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include <iostream>
#include <string>
#include <vector>

#include "detail/EscapeSequenceParser.h"

namespace {

int failures = 0;

void check(const bool condition, const char* expression, const int line) {
    if (!condition) {
        std::cerr << "FAILED line " << line << ": " << expression << '\n';
        failures++;
    }
}

#define TT_CHECK(expression) check((expression), #expression, __LINE__)

void testAsciiAndUtf8() {
    tt::detail::EscapeSequenceParser parser;
    std::vector<tt::detail::NativeInputEvent> events;
    parser.feed("aş", events);

    TT_CHECK(events.size() == 6);
    TT_CHECK(events[0].type == tt::detail::NativeInputEventType::KeyDown);
    TT_CHECK(events[0].key == tt::Key::A);
    TT_CHECK(events[2].type == tt::detail::NativeInputEventType::Text);
    TT_CHECK(events[2].character == U'a');
    TT_CHECK(events[5].character == U'ş');
}

void testNavigationAndModifiers() {
    tt::detail::EscapeSequenceParser parser;
    std::vector<tt::detail::NativeInputEvent> events;
    parser.feed("\x1b[A\x1b[1;5C\x1b[15~", events);

    TT_CHECK(events.size() == 6);
    TT_CHECK(events[0].key == tt::Key::Up);
    TT_CHECK(events[2].key == tt::Key::Right);
    TT_CHECK(events[2].modifiers.control());
    TT_CHECK(events[4].key == tt::Key::F5);
}

void testFocusAndPartialSequences() {
    tt::detail::EscapeSequenceParser parser;
    std::vector<tt::detail::NativeInputEvent> events;

    parser.feed("\x1b[", events);
    TT_CHECK(events.empty());
    parser.feed("I\x1b[O", events);
    TT_CHECK(events.size() == 2);
    TT_CHECK(events[0].type == tt::detail::NativeInputEventType::FocusGained);
    TT_CHECK(events[1].type == tt::detail::NativeInputEventType::FocusLost);
}

void testAltAndEscape() {
    tt::detail::EscapeSequenceParser parser;
    std::vector<tt::detail::NativeInputEvent> events;

    parser.feed("\x1bx", events);
    TT_CHECK(events.size() == 3);
    TT_CHECK(events[0].key == tt::Key::X);
    TT_CHECK(events[0].modifiers.alt());

    events.clear();
    parser.feed("\x1b", events);
    TT_CHECK(events.empty());
    parser.flush(events);
    TT_CHECK(events.size() == 2);
    TT_CHECK(events[0].key == tt::Key::Escape);
}

void testControlKey() {
    tt::detail::EscapeSequenceParser parser;
    std::vector<tt::detail::NativeInputEvent> events;
    parser.feed(std::string(1, static_cast<char>(3)), events);

    TT_CHECK(events.size() == 2);
    TT_CHECK(events[0].key == tt::Key::C);
    TT_CHECK(events[0].modifiers.control());
}

} // namespace

int main() {
    testAsciiAndUtf8();
    testNavigationAndModifiers();
    testFocusAndPartialSequences();
    testAltAndEscape();
    testControlKey();

    if (failures != 0) {
        std::cerr << failures << " parser test(s) failed.\n";
        return 1;
    }

    std::cout << "All escape-sequence parser tests passed.\n";
    return 0;
}
