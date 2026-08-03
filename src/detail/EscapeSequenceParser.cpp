/**
 * @file EscapeSequenceParser.cpp
 * @brief Implements the shared POSIX terminal escape-sequence parser.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "detail/EscapeSequenceParser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using tt::Key;
using tt::ModifierState;
using tt::detail::NativeInputEvent;
using tt::detail::NativeInputEventType;

constexpr auto ESCAPE_DELAY = std::chrono::milliseconds(25);

void appendKeyPulse(
    std::vector<NativeInputEvent>& events,
    const Key key,
    const ModifierState& modifiers = {},
    const std::uint32_t nativeCode = 0
) {
    NativeInputEvent down;
    down.type = NativeInputEventType::KeyDown;
    down.key = key;
    down.nativeKeyCode = nativeCode;
    down.modifiers = modifiers;
    events.push_back(down);

    NativeInputEvent up = down;
    up.type = NativeInputEventType::KeyUp;
    events.push_back(up);
}

void appendText(std::vector<NativeInputEvent>& events, const char32_t character, const ModifierState& modifiers = {}) {
    NativeInputEvent event;
    event.type = NativeInputEventType::Text;
    event.character = character;
    event.nativeKeyCode = static_cast<std::uint32_t>(character);
    event.modifiers = modifiers;
    events.push_back(event);
}

ModifierState modifiersFromParameter(const int parameter) {
    ModifierState modifiers;
    const int bits = std::max(1, parameter) - 1;
    modifiers.leftShift = (bits & 1) != 0;
    modifiers.leftAlt = (bits & 2) != 0;
    modifiers.leftControl = (bits & 4) != 0;
    modifiers.leftSuper = (bits & 8) != 0;
    return modifiers;
}

std::vector<int> parseParameters(const std::string& text) {
    std::vector<int> parameters;
    std::size_t start = 0;

    while (start <= text.size()) {
        const std::size_t end = text.find(';', start);
        const std::string token = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        parameters.push_back(token.empty() ? 0 : std::atoi(token.c_str()));

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    return parameters;
}

Key keyFromAscii(const char32_t character) {
    if (character >= U'a' && character <= U'z') {
        return static_cast<Key>(static_cast<std::size_t>(Key::A) + static_cast<std::size_t>(character - U'a'));
    }
    if (character >= U'A' && character <= U'Z') {
        return static_cast<Key>(static_cast<std::size_t>(Key::A) + static_cast<std::size_t>(character - U'A'));
    }
    if (character >= U'0' && character <= U'9') {
        return static_cast<Key>(static_cast<std::size_t>(Key::Zero) + static_cast<std::size_t>(character - U'0'));
    }

    switch (character) {
        case U' ': return Key::Space;
        case U';': case U':': return Key::Oem1;
        case U'=': case U'+': return Key::OemPlus;
        case U',': case U'<': return Key::OemComma;
        case U'-': case U'_': return Key::OemMinus;
        case U'.': case U'>': return Key::OemPeriod;
        case U'/': case U'?': return Key::Oem2;
        case U'`': case U'~': return Key::Oem3;
        case U'[': case U'{': return Key::Oem4;
        case U'\\': case U'|': return Key::Oem5;
        case U']': case U'}': return Key::Oem6;
        case U'\'': case U'"': return Key::Oem7;
        default: return Key::Unknown;
    }
}

bool isShiftedAscii(const char32_t character) {
    return
        (character >= U'A' && character <= U'Z') ||
        character == U'!' || character == U'@' || character == U'#' || character == U'$' ||
        character == U'%' || character == U'^' || character == U'&' || character == U'*' ||
        character == U'(' || character == U')' || character == U'_' || character == U'+' ||
        character == U'{' || character == U'}' || character == U'|' || character == U':' ||
        character == U'"' || character == U'<' || character == U'>' || character == U'?' ||
        character == U'~';
}

bool decodeUtf8At(const std::string& bytes, const std::size_t offset, char32_t& character, std::size_t& length) {
    if (offset >= bytes.size()) {
        return false;
    }

    const auto first = static_cast<unsigned char>(bytes[offset]);
    if (first < 0x80) {
        character = first;
        length = 1;
        return true;
    }

    if ((first & 0xE0) == 0xC0) {
        length = 2;
        character = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        length = 3;
        character = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        length = 4;
        character = first & 0x07;
    } else {
        character = 0xFFFD;
        length = 1;
        return true;
    }

    if (offset + length > bytes.size()) {
        return false;
    }

    for (std::size_t index = 1; index < length; index++) {
        const auto continuation = static_cast<unsigned char>(bytes[offset + index]);
        if ((continuation & 0xC0) != 0x80) {
            character = 0xFFFD;
            length = 1;
            return true;
        }
        character = (character << 6) | (continuation & 0x3F);
    }

    const bool overlong =
        (length == 2 && character < 0x80) ||
        (length == 3 && character < 0x800) ||
        (length == 4 && character < 0x10000);
    const bool invalidScalar =
        character > 0x10FFFF ||
        (character >= 0xD800 && character <= 0xDFFF);

    if (overlong || invalidScalar) {
        character = 0xFFFD;
    }

    return true;
}

void appendCharacterPulse(
    std::vector<NativeInputEvent>& events,
    const char32_t character,
    ModifierState modifiers = {}
) {
    if (isShiftedAscii(character)) {
        modifiers.leftShift = true;
    }
    appendKeyPulse(events, keyFromAscii(character), modifiers, static_cast<std::uint32_t>(character));
    appendText(events, character, modifiers);
}

Key keyFromTildeCode(const int code) {
    switch (code) {
        case 1: case 7: return Key::Home;
        case 2: return Key::Insert;
        case 3: return Key::Delete;
        case 4: case 8: return Key::End;
        case 5: return Key::PageUp;
        case 6: return Key::PageDown;
        case 11: return Key::F1;
        case 12: return Key::F2;
        case 13: return Key::F3;
        case 14: return Key::F4;
        case 15: return Key::F5;
        case 17: return Key::F6;
        case 18: return Key::F7;
        case 19: return Key::F8;
        case 20: return Key::F9;
        case 21: return Key::F10;
        case 23: return Key::F11;
        case 24: return Key::F12;
        case 25: return Key::F13;
        case 26: return Key::F14;
        case 28: return Key::F15;
        case 29: return Key::F16;
        case 31: return Key::F17;
        case 32: return Key::F18;
        case 33: return Key::F19;
        case 34: return Key::F20;
        default: return Key::Unknown;
    }
}

Key keyFromFinal(const char finalByte) {
    switch (finalByte) {
        case 'A': return Key::Up;
        case 'B': return Key::Down;
        case 'C': return Key::Right;
        case 'D': return Key::Left;
        case 'H': return Key::Home;
        case 'F': return Key::End;
        case 'P': return Key::F1;
        case 'Q': return Key::F2;
        case 'R': return Key::F3;
        case 'S': return Key::F4;
        default: return Key::Unknown;
    }
}

void parseCsi(const std::string& sequence, std::vector<NativeInputEvent>& events) {
    if (sequence.size() < 3) {
        return;
    }

    const char finalByte = sequence.back();
    const std::string body = sequence.substr(2, sequence.size() - 3);

    if (body.empty() && finalByte == 'I') {
        NativeInputEvent event;
        event.type = NativeInputEventType::FocusGained;
        events.push_back(event);
        return;
    }
    if (body.empty() && finalByte == 'O') {
        NativeInputEvent event;
        event.type = NativeInputEventType::FocusLost;
        events.push_back(event);
        return;
    }

    const std::vector<int> parameters = parseParameters(body);
    const int modifierParameter = parameters.size() >= 2 ? parameters[1] : 1;
    const ModifierState modifiers = modifiersFromParameter(modifierParameter);

    Key key = Key::Unknown;
    if (finalByte == '~') {
        key = keyFromTildeCode(parameters.empty() ? 0 : parameters[0]);
    } else {
        key = keyFromFinal(finalByte);
    }

    if (key != Key::Unknown) {
        appendKeyPulse(events, key, modifiers, static_cast<std::uint32_t>(finalByte));
    }
}

void parseSs3(const std::string& sequence, std::vector<NativeInputEvent>& events) {
    if (sequence.size() != 3) {
        return;
    }
    const Key key = keyFromFinal(sequence[2]);
    if (key != Key::Unknown) {
        appendKeyPulse(events, key, {}, static_cast<std::uint32_t>(sequence[2]));
    }
}

} // namespace

namespace tt::detail {

void EscapeSequenceParser::feed(
    const char* bytes,
    const std::size_t size,
    std::vector<NativeInputEvent>& events
) {
    if (bytes == nullptr || size == 0) {
        return;
    }
    buffer.append(bytes, size);
    parseAvailable(events, false);
}

void EscapeSequenceParser::feed(const std::string& bytes, std::vector<NativeInputEvent>& events) {
    feed(bytes.data(), bytes.size(), events);
}

void EscapeSequenceParser::flushExpired(std::vector<NativeInputEvent>& events) {
    if (
        escapePending &&
        std::chrono::steady_clock::now() - escapePendingSince >= ESCAPE_DELAY
    ) {
        parseAvailable(events, true);
    }
}

void EscapeSequenceParser::flush(std::vector<NativeInputEvent>& events) {
    parseAvailable(events, true);

    while (!buffer.empty()) {
        char32_t character = 0;
        std::size_t length = 0;
        if (!decodeUtf8At(buffer, 0, character, length)) {
            character = 0xFFFD;
            length = 1;
        }
        appendCharacterPulse(events, character);
        buffer.erase(0, length);
    }

    escapePending = false;
}

void EscapeSequenceParser::reset() noexcept {
    buffer.clear();
    escapePending = false;
    escapePendingSince = {};
}

void EscapeSequenceParser::parseAvailable(
    std::vector<NativeInputEvent>& events,
    const bool forceEscape
) {
    std::size_t consumed = 0;

    while (consumed < buffer.size()) {
        const auto byte = static_cast<unsigned char>(buffer[consumed]);

        if (byte == 0x1B) {
            if (consumed + 1 >= buffer.size()) {
                if (!forceEscape) {
                    if (!escapePending) {
                        escapePending = true;
                        escapePendingSince = std::chrono::steady_clock::now();
                    }
                    break;
                }
                appendKeyPulse(events, Key::Escape, {}, 0x1B);
                consumed++;
                escapePending = false;
                continue;
            }

            escapePending = false;
            const char next = buffer[consumed + 1];

            if (next == '[') {
                std::size_t finalPosition = consumed + 2;
                while (
                    finalPosition < buffer.size() &&
                    (static_cast<unsigned char>(buffer[finalPosition]) < 0x40 ||
                     static_cast<unsigned char>(buffer[finalPosition]) > 0x7E)
                ) {
                    finalPosition++;
                }

                if (finalPosition >= buffer.size()) {
                    break;
                }

                parseCsi(buffer.substr(consumed, finalPosition - consumed + 1), events);
                consumed = finalPosition + 1;
                continue;
            }

            if (next == 'O') {
                if (consumed + 2 >= buffer.size()) {
                    break;
                }
                parseSs3(buffer.substr(consumed, 3), events);
                consumed += 3;
                continue;
            }

            char32_t character = 0;
            std::size_t length = 0;
            if (!decodeUtf8At(buffer, consumed + 1, character, length)) {
                break;
            }
            ModifierState modifiers;
            modifiers.leftAlt = true;
            appendCharacterPulse(events, character, modifiers);
            consumed += 1 + length;
            continue;
        }

        if (byte == '\r' || byte == '\n') {
            appendKeyPulse(events, Key::Enter, {}, byte);
            consumed++;
            continue;
        }
        if (byte == '\t') {
            appendKeyPulse(events, Key::Tab, {}, byte);
            consumed++;
            continue;
        }
        if (byte == 0x7F || byte == 0x08) {
            appendKeyPulse(events, Key::Backspace, {}, byte);
            consumed++;
            continue;
        }
        if (byte >= 0x01 && byte <= 0x1A) {
            ModifierState modifiers;
            modifiers.leftControl = true;
            const Key key = static_cast<Key>(
                static_cast<std::size_t>(Key::A) + static_cast<std::size_t>(byte - 1)
            );
            appendKeyPulse(events, key, modifiers, byte);
            consumed++;
            continue;
        }

        char32_t character = 0;
        std::size_t length = 0;
        if (!decodeUtf8At(buffer, consumed, character, length)) {
            break;
        }
        appendCharacterPulse(events, character);
        consumed += length;
    }

    if (consumed > 0) {
        buffer.erase(0, consumed);
    }
}

} // namespace tt::detail
