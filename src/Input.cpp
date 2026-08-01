/**
 * @file Input.cpp
 * @brief Implements comprehensive tt::Input console event handling.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/Input.h"

#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tt {

std::array<bool, Input::KEY_COUNT> Input::current {};
std::array<bool, Input::KEY_COUNT> Input::pressed {};
std::array<bool, Input::KEY_COUNT> Input::released {};
std::string Input::text;
char16_t Input::pendingHighSurrogate = 0;
bool Input::focused = true;

std::size_t Input::index(const Key key) noexcept {
    return static_cast<std::size_t>(key);
}

void Input::initialize() {
    reset();

#ifdef _WIN32
    const HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);

    if (inputHandle != INVALID_HANDLE_VALUE && inputHandle != nullptr) {
        FlushConsoleInputBuffer(inputHandle);
    }
#endif
}

void Input::update() {
    pressed.fill(false);
    released.fill(false);
    text.clear();

#ifdef _WIN32
    const HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);

    if (inputHandle == INVALID_HANDLE_VALUE || inputHandle == nullptr) {
        return;
    }

    constexpr DWORD MAX_EVENTS = 128;
    INPUT_RECORD events[MAX_EVENTS] {};

    while (true) {
        DWORD pendingEvents = 0;

        if (!GetNumberOfConsoleInputEvents(inputHandle, &pendingEvents) || pendingEvents == 0) {
            break;
        }

        const DWORD eventsToRead = std::min(pendingEvents, MAX_EVENTS);
        DWORD eventsRead = 0;

        if (!ReadConsoleInputW(inputHandle, events, eventsToRead, &eventsRead)) {
            break;
        }

        for (DWORD i = 0; i < eventsRead; i++) {
            const INPUT_RECORD& inputEvent = events[i];

            if (inputEvent.EventType == KEY_EVENT) {
                const KEY_EVENT_RECORD& keyEvent = inputEvent.Event.KeyEvent;
                const Key key = keyFromVirtualCode(
                    keyEvent.wVirtualKeyCode,
                    keyEvent.wVirtualScanCode,
                    keyEvent.dwControlKeyState
                );

                if (key != Key::Count) {
                    setKeyState(key, keyEvent.bKeyDown != FALSE);
                }

                if (keyEvent.bKeyDown != FALSE && keyEvent.uChar.UnicodeChar != 0) {
                    appendTextCodeUnit(
                        static_cast<char16_t>(keyEvent.uChar.UnicodeChar),
                        std::max<std::uint16_t>(1, keyEvent.wRepeatCount)
                    );
                }
            } else if (inputEvent.EventType == FOCUS_EVENT) {
                focused = inputEvent.Event.FocusEvent.bSetFocus != FALSE;

                if (!focused) {
                    releaseAllKeys();
                    pendingHighSurrogate = 0;
                }
            }
        }
    }
#else
    focused = true;
#endif
}

void Input::reset() noexcept {
    current.fill(false);
    pressed.fill(false);
    released.fill(false);
    text.clear();
    pendingHighSurrogate = 0;
    focused = true;
}

bool Input::isHeld(const Key key) noexcept {
    const std::size_t keyIndex = index(key);
    return keyIndex < KEY_COUNT && current[keyIndex];
}

bool Input::isPressed(const Key key) noexcept {
    const std::size_t keyIndex = index(key);
    return keyIndex < KEY_COUNT && pressed[keyIndex];
}

bool Input::isReleased(const Key key) noexcept {
    const std::size_t keyIndex = index(key);
    return keyIndex < KEY_COUNT && released[keyIndex];
}

bool Input::isFocused() noexcept {
    return focused;
}

const std::string& Input::textInput() noexcept {
    return text;
}

Key Input::keyFromVirtualCode(
    std::uint16_t virtualCode,
    const std::uint16_t scanCode,
    const std::uint32_t controlState
) {
#ifdef _WIN32
    if (virtualCode == VK_SHIFT) {
        virtualCode = static_cast<std::uint16_t>(MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX));
    } else if (virtualCode == VK_CONTROL) {
        virtualCode = (controlState & ENHANCED_KEY) != 0 ? VK_RCONTROL : VK_LCONTROL;
    } else if (virtualCode == VK_MENU) {
        virtualCode = (controlState & ENHANCED_KEY) != 0 ? VK_RMENU : VK_LMENU;
    } else if (virtualCode == VK_RETURN && (controlState & ENHANCED_KEY) != 0) {
        return Key::NumpadEnter;
    }

    switch (virtualCode) {
        case VK_CANCEL: return Key::Cancel;
        case VK_ESCAPE: return Key::Escape;
        case VK_BACK: return Key::Backspace;
        case VK_TAB: return Key::Tab;
        case VK_CLEAR: return Key::Clear;
        case VK_RETURN: return Key::Enter;
        case VK_PAUSE: return Key::Pause;
        case VK_CAPITAL: return Key::CapsLock;
        case VK_KANA: return Key::KanaHangul;
        case VK_JUNJA: return Key::Junja;
        case VK_FINAL: return Key::Final;
        case VK_HANJA: return Key::HanjaKanji;
#ifdef VK_IME_ON
        case VK_IME_ON: return Key::ImeOn;
#endif
#ifdef VK_IME_OFF
        case VK_IME_OFF: return Key::ImeOff;
#endif
        case VK_CONVERT: return Key::Convert;
        case VK_NONCONVERT: return Key::NonConvert;
        case VK_ACCEPT: return Key::Accept;
        case VK_MODECHANGE: return Key::ModeChange;
        case VK_SPACE: return Key::Space;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_END: return Key::End;
        case VK_HOME: return Key::Home;
        case VK_LEFT: return Key::Left;
        case VK_UP: return Key::Up;
        case VK_RIGHT: return Key::Right;
        case VK_DOWN: return Key::Down;
        case VK_SELECT: return Key::Select;
        case VK_PRINT: return Key::Print;
        case VK_EXECUTE: return Key::Execute;
        case VK_SNAPSHOT: return Key::PrintScreen;
        case VK_INSERT: return Key::Insert;
        case VK_DELETE: return Key::Delete;
        case VK_HELP: return Key::Help;

        case '0': return Key::Zero;
        case '1': return Key::One;
        case '2': return Key::Two;
        case '3': return Key::Three;
        case '4': return Key::Four;
        case '5': return Key::Five;
        case '6': return Key::Six;
        case '7': return Key::Seven;
        case '8': return Key::Eight;
        case '9': return Key::Nine;

        case 'A': return Key::A;
        case 'B': return Key::B;
        case 'C': return Key::C;
        case 'D': return Key::D;
        case 'E': return Key::E;
        case 'F': return Key::F;
        case 'G': return Key::G;
        case 'H': return Key::H;
        case 'I': return Key::I;
        case 'J': return Key::J;
        case 'K': return Key::K;
        case 'L': return Key::L;
        case 'M': return Key::M;
        case 'N': return Key::N;
        case 'O': return Key::O;
        case 'P': return Key::P;
        case 'Q': return Key::Q;
        case 'R': return Key::R;
        case 'S': return Key::S;
        case 'T': return Key::T;
        case 'U': return Key::U;
        case 'V': return Key::V;
        case 'W': return Key::W;
        case 'X': return Key::X;
        case 'Y': return Key::Y;
        case 'Z': return Key::Z;

        case VK_LWIN: return Key::LeftWindows;
        case VK_RWIN: return Key::RightWindows;
        case VK_APPS: return Key::Menu;
        case VK_SLEEP: return Key::Sleep;

        case VK_NUMPAD0: return Key::Numpad0;
        case VK_NUMPAD1: return Key::Numpad1;
        case VK_NUMPAD2: return Key::Numpad2;
        case VK_NUMPAD3: return Key::Numpad3;
        case VK_NUMPAD4: return Key::Numpad4;
        case VK_NUMPAD5: return Key::Numpad5;
        case VK_NUMPAD6: return Key::Numpad6;
        case VK_NUMPAD7: return Key::Numpad7;
        case VK_NUMPAD8: return Key::Numpad8;
        case VK_NUMPAD9: return Key::Numpad9;
        case VK_MULTIPLY: return Key::NumpadMultiply;
        case VK_ADD: return Key::NumpadAdd;
        case VK_SEPARATOR: return Key::NumpadSeparator;
        case VK_SUBTRACT: return Key::NumpadSubtract;
        case VK_DECIMAL: return Key::NumpadDecimal;
        case VK_DIVIDE: return Key::NumpadDivide;
#ifdef VK_OEM_NEC_EQUAL
        case VK_OEM_NEC_EQUAL: return Key::NumpadEqual;
#endif

        case VK_F1: return Key::F1;
        case VK_F2: return Key::F2;
        case VK_F3: return Key::F3;
        case VK_F4: return Key::F4;
        case VK_F5: return Key::F5;
        case VK_F6: return Key::F6;
        case VK_F7: return Key::F7;
        case VK_F8: return Key::F8;
        case VK_F9: return Key::F9;
        case VK_F10: return Key::F10;
        case VK_F11: return Key::F11;
        case VK_F12: return Key::F12;
        case VK_F13: return Key::F13;
        case VK_F14: return Key::F14;
        case VK_F15: return Key::F15;
        case VK_F16: return Key::F16;
        case VK_F17: return Key::F17;
        case VK_F18: return Key::F18;
        case VK_F19: return Key::F19;
        case VK_F20: return Key::F20;
        case VK_F21: return Key::F21;
        case VK_F22: return Key::F22;
        case VK_F23: return Key::F23;
        case VK_F24: return Key::F24;

        case VK_NUMLOCK: return Key::NumLock;
        case VK_SCROLL: return Key::ScrollLock;
        case VK_LSHIFT: return Key::LeftShift;
        case VK_RSHIFT: return Key::RightShift;
        case VK_LCONTROL: return Key::LeftControl;
        case VK_RCONTROL: return Key::RightControl;
        case VK_LMENU: return Key::LeftAlt;
        case VK_RMENU: return Key::RightAlt;

        case VK_OEM_1: return Key::Semicolon;
        case VK_OEM_PLUS: return Key::Equal;
        case VK_OEM_COMMA: return Key::Comma;
        case VK_OEM_MINUS: return Key::Minus;
        case VK_OEM_PERIOD: return Key::Period;
        case VK_OEM_2: return Key::Slash;
        case VK_OEM_3: return Key::Grave;
        case VK_OEM_4: return Key::LeftBracket;
        case VK_OEM_5: return Key::Backslash;
        case VK_OEM_6: return Key::RightBracket;
        case VK_OEM_7: return Key::Apostrophe;
        case VK_OEM_8: return Key::Oem8;
        case VK_OEM_102: return Key::NonUsBackslash;
        case VK_OEM_CLEAR: return Key::OemClear;

        case VK_BROWSER_BACK: return Key::BrowserBack;
        case VK_BROWSER_FORWARD: return Key::BrowserForward;
        case VK_BROWSER_REFRESH: return Key::BrowserRefresh;
        case VK_BROWSER_STOP: return Key::BrowserStop;
        case VK_BROWSER_SEARCH: return Key::BrowserSearch;
        case VK_BROWSER_FAVORITES: return Key::BrowserFavourites;
        case VK_BROWSER_HOME: return Key::BrowserHome;
        case VK_VOLUME_MUTE: return Key::VolumeMute;
        case VK_VOLUME_DOWN: return Key::VolumeDown;
        case VK_VOLUME_UP: return Key::VolumeUp;
        case VK_MEDIA_NEXT_TRACK: return Key::MediaNextTrack;
        case VK_MEDIA_PREV_TRACK: return Key::MediaPreviousTrack;
        case VK_MEDIA_STOP: return Key::MediaStop;
        case VK_MEDIA_PLAY_PAUSE: return Key::MediaPlayPause;
        case VK_LAUNCH_MAIL: return Key::LaunchMail;
        case VK_LAUNCH_MEDIA_SELECT: return Key::LaunchMediaSelect;
        case VK_LAUNCH_APP1: return Key::LaunchApp1;
        case VK_LAUNCH_APP2: return Key::LaunchApp2;

        case VK_PROCESSKEY: return Key::Process;
        case VK_PACKET: return Key::Packet;
        case VK_ATTN: return Key::Attn;
        case VK_CRSEL: return Key::CrSel;
        case VK_EXSEL: return Key::ExSel;
        case VK_EREOF: return Key::EraseEof;
        case VK_PLAY: return Key::Play;
        case VK_ZOOM: return Key::Zoom;
        case VK_NONAME: return Key::NoName;
        case VK_PA1: return Key::Pa1;
        default: return Key::Count;
    }
#else
    (void) virtualCode;
    (void) scanCode;
    (void) controlState;
    return Key::Count;
#endif
}

void Input::setKeyState(const Key key, const bool isDown) {
    const std::size_t keyIndex = index(key);

    if (keyIndex >= KEY_COUNT) {
        return;
    }

    if (isDown) {
        if (!current[keyIndex]) {
            pressed[keyIndex] = true;
        }

        current[keyIndex] = true;
        return;
    }

    if (current[keyIndex]) {
        released[keyIndex] = true;
    }

    current[keyIndex] = false;
}

void Input::releaseAllKeys() {
    for (std::size_t i = 0; i < KEY_COUNT; i++) {
        if (current[i]) {
            released[i] = true;
            current[i] = false;
        }
    }
}

void Input::appendTextCodeUnit(const char16_t codeUnit, const std::uint16_t repeatCount) {
    for (std::uint16_t repeat = 0; repeat < repeatCount; repeat++) {
        if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF) {
            pendingHighSurrogate = codeUnit;
            continue;
        }

        char32_t character = codeUnit;

        if (codeUnit >= 0xDC00 && codeUnit <= 0xDFFF && pendingHighSurrogate != 0) {
            character =
                0x10000 +
                ((static_cast<char32_t>(pendingHighSurrogate) - 0xD800) << 10) +
                (static_cast<char32_t>(codeUnit) - 0xDC00);
        } else if (pendingHighSurrogate != 0) {
            appendUtf8(U'�');
        }

        pendingHighSurrogate = 0;

        if (character >= U' ' && character != 0x7F) {
            appendUtf8(character);
        }
    }
}

void Input::appendUtf8(const char32_t character) {
    if (character <= 0x7F) {
        text.push_back(static_cast<char>(character));
    } else if (character <= 0x7FF) {
        text.push_back(static_cast<char>(0xC0 | ((character >> 6) & 0x1F)));
        text.push_back(static_cast<char>(0x80 | (character & 0x3F)));
    } else if (character <= 0xFFFF) {
        text.push_back(static_cast<char>(0xE0 | ((character >> 12) & 0x0F)));
        text.push_back(static_cast<char>(0x80 | ((character >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (character & 0x3F)));
    } else if (character <= 0x10FFFF) {
        text.push_back(static_cast<char>(0xF0 | ((character >> 18) & 0x07)));
        text.push_back(static_cast<char>(0x80 | ((character >> 12) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | ((character >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (character & 0x3F)));
    }
}

} // namespace tt
