/**
 * @file Input.cpp
 * @brief Implements cross-platform input state and the bounded raw event queue.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/Input.h"

#include "detail/NativeInputEvent.h"
#include "detail/Unicode.h"
#include "platform/PlatformTerminal.h"
#include "terminalTool/TerminalError.h"
#include "terminalTool/TerminalSession.h"

#include <utility>
#include <vector>

namespace {

std::vector<tt::detail::NativeInputEvent> nativeEventBuffer;

} // namespace

namespace tt {

std::array<bool, Input::KEY_COUNT> Input::current {};
std::array<bool, Input::KEY_COUNT> Input::pressed {};
std::array<bool, Input::KEY_COUNT> Input::released {};
std::string Input::text;
std::deque<InputEvent> Input::eventQueue;
std::size_t Input::maximumEventQueueSize = 4096;
std::size_t Input::droppedEvents = 0;
bool Input::queueOverflowed = false;
bool Input::focused = true;
bool Input::gainedFocus = false;
bool Input::lostFocus = false;

bool ModifierState::shift() const noexcept {
    return leftShift || rightShift;
}

bool ModifierState::control() const noexcept {
    return leftControl || rightControl;
}

bool ModifierState::alt() const noexcept {
    return leftAlt || rightAlt;
}

bool ModifierState::super() const noexcept {
    return leftSuper || rightSuper;
}

bool ModifierState::altGr() const noexcept {
    return rightAlt;
}

std::size_t Input::index(const Key key) noexcept {
    return static_cast<std::size_t>(key);
}

void Input::initialize(const std::size_t maximumQueuedEvents) {
    reset();
    maximumEventQueueSize = maximumQueuedEvents;
    nativeEventBuffer.reserve(128);
    detail::platformFlushInput();
}

void Input::reset() noexcept {
    current.fill(false);
    pressed.fill(false);
    released.fill(false);
    text.clear();
    eventQueue.clear();
    droppedEvents = 0;
    queueOverflowed = false;
    focused = true;
    gainedFocus = false;
    lostFocus = false;
    nativeEventBuffer.clear();
}

void Input::update() {
    if (!TerminalSession::hasActiveSession() || !detail::platformIsInitialized()) {
        throw TerminalError(
            TerminalErrorCode::NoActiveSession,
            "tt::Input::update() requires an active TerminalSession."
        );
    }

    pressed.fill(false);
    released.fill(false);
    text.clear();
    gainedFocus = false;
    lostFocus = false;

    nativeEventBuffer.clear();
    detail::platformReadInput(nativeEventBuffer);

    for (const detail::NativeInputEvent& event : nativeEventBuffer) {
        processNativeEvent(event);
    }
}

void Input::processNativeEvent(const detail::NativeInputEvent& event) {
    switch (event.type) {
        case detail::NativeInputEventType::KeyDown: {
            KeyEventData key;
            key.key = event.key;
            key.repeated = event.repeated;
            key.repeatCount = event.repeatCount;
            key.scanCode = event.scanCode;
            key.nativeKeyCode = event.nativeKeyCode;
            key.modifiers = event.modifiers;
            setKeyState(key, true);
            break;
        }

        case detail::NativeInputEventType::KeyUp: {
            KeyEventData key;
            key.key = event.key;
            key.repeated = false;
            key.repeatCount = event.repeatCount;
            key.scanCode = event.scanCode;
            key.nativeKeyCode = event.nativeKeyCode;
            key.modifiers = event.modifiers;
            setKeyState(key, false);
            break;
        }

        case detail::NativeInputEventType::Text: {
            appendUtf8(event.character);
            InputEvent publicEvent;
            publicEvent.type = InputEventType::TextEntered;
            publicEvent.character = detail::isValidUnicodeScalar(event.character)
                ? event.character
                : U'\uFFFD';
            publicEvent.key.nativeKeyCode = event.nativeKeyCode;
            publicEvent.key.modifiers = event.modifiers;
            enqueue(std::move(publicEvent));
            break;
        }

        case detail::NativeInputEventType::FocusGained: {
            focused = true;
            gainedFocus = true;
            InputEvent publicEvent;
            publicEvent.type = InputEventType::FocusGained;
            enqueue(std::move(publicEvent));
            break;
        }

        case detail::NativeInputEventType::FocusLost: {
            focused = false;
            lostFocus = true;
            releaseAllKeys(event.modifiers);
            InputEvent publicEvent;
            publicEvent.type = InputEventType::FocusLost;
            enqueue(std::move(publicEvent));
            break;
        }
    }
}

void Input::setKeyState(KeyEventData key, const bool isDown) {
    const std::size_t keyIndex = index(key.key);
    const bool known = key.key != Key::Unknown && keyIndex < KEY_COUNT;

    if (isDown) {
        if (known) {
            if (!current[keyIndex]) {
                pressed[keyIndex] = true;
            } else {
                key.repeated = true;
            }
            current[keyIndex] = true;
        }

        InputEvent event;
        event.type = InputEventType::KeyPressed;
        event.key = key;
        enqueue(std::move(event));
        return;
    }

    if (known) {
        if (current[keyIndex]) {
            released[keyIndex] = true;
        }
        current[keyIndex] = false;
    }

    InputEvent event;
    event.type = InputEventType::KeyReleased;
    event.key = key;
    enqueue(std::move(event));
}

void Input::releaseAllKeys(const ModifierState& modifiers) {
    for (std::size_t keyIndex = 0; keyIndex < KEY_COUNT; keyIndex++) {
        if (!current[keyIndex]) {
            continue;
        }

        current[keyIndex] = false;
        released[keyIndex] = true;

        InputEvent event;
        event.type = InputEventType::KeyReleased;
        event.key.key = static_cast<Key>(keyIndex);
        event.key.modifiers = modifiers;
        enqueue(std::move(event));
    }
}

void Input::appendUtf8(const char32_t character) {
    text += detail::encodeUtf8(character);
}

void Input::enqueue(InputEvent event) noexcept {
    if (maximumEventQueueSize == 0) {
        return;
    }

    if (eventQueue.size() >= maximumEventQueueSize) {
        eventQueue.pop_front();
        droppedEvents++;
        queueOverflowed = true;
    }

    try {
        eventQueue.push_back(std::move(event));
    } catch (...) {
        droppedEvents++;
        queueOverflowed = true;
    }
}

bool Input::isHeld(const Key key) noexcept {
    const std::size_t keyIndex = index(key);
    return key != Key::Unknown && keyIndex < KEY_COUNT && current[keyIndex];
}

bool Input::isPressed(const Key key) noexcept {
    const std::size_t keyIndex = index(key);
    return key != Key::Unknown && keyIndex < KEY_COUNT && pressed[keyIndex];
}

bool Input::isReleased(const Key key) noexcept {
    const std::size_t keyIndex = index(key);
    return key != Key::Unknown && keyIndex < KEY_COUNT && released[keyIndex];
}

bool Input::isFocused() noexcept {
    return focused;
}

bool Input::focusGained() noexcept {
    return gainedFocus;
}

bool Input::focusLost() noexcept {
    return lostFocus;
}

const std::string& Input::textInput() noexcept {
    return text;
}

std::size_t Input::eventCount() noexcept {
    return eventQueue.size();
}

bool Input::hasEvent() noexcept {
    return !eventQueue.empty();
}

std::optional<InputEvent> Input::pollEvent() {
    if (eventQueue.empty()) {
        return std::nullopt;
    }

    InputEvent event = std::move(eventQueue.front());
    eventQueue.pop_front();
    return event;
}

void Input::clearEvents() noexcept {
    eventQueue.clear();
}

std::size_t Input::droppedEventCount() noexcept {
    return droppedEvents;
}

bool Input::eventOverflowed() noexcept {
    return queueOverflowed;
}

void Input::clearEventOverflow() noexcept {
    droppedEvents = 0;
    queueOverflowed = false;
}

} // namespace tt
