/**
 * @file Unicode.cpp
 * @brief Implements strict Unicode scalar and UTF-8 helpers.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "detail/Unicode.h"

namespace tt::detail {

bool isValidUnicodeScalar(const char32_t character) noexcept {
    return
        character <= 0x10FFFF &&
        !(character >= 0xD800 && character <= 0xDFFF);
}

Utf8DecodeResult decodeNextUtf8(const std::string_view bytes, const std::size_t offset) noexcept {
    if (offset >= bytes.size()) {
        return {};
    }

    const auto first = static_cast<unsigned char>(bytes[offset]);
    if (first < 0x80) {
        return Utf8DecodeResult { static_cast<char32_t>(first), 1, true };
    }

    std::size_t length = 0;
    char32_t character = 0;
    char32_t minimum = 0;

    if (first >= 0xC2 && first <= 0xDF) {
        length = 2;
        character = static_cast<char32_t>(first & 0x1F);
        minimum = 0x80;
    } else if (first >= 0xE0 && first <= 0xEF) {
        length = 3;
        character = static_cast<char32_t>(first & 0x0F);
        minimum = 0x800;
    } else if (first >= 0xF0 && first <= 0xF4) {
        length = 4;
        character = static_cast<char32_t>(first & 0x07);
        minimum = 0x10000;
    } else {
        return Utf8DecodeResult { U'\uFFFD', 1, true };
    }

    if (offset + length > bytes.size()) {
        return Utf8DecodeResult { U'\uFFFD', 0, false };
    }

    for (std::size_t index = 1; index < length; index++) {
        const auto continuation = static_cast<unsigned char>(bytes[offset + index]);
        if ((continuation & 0xC0) != 0x80) {
            return Utf8DecodeResult { U'\uFFFD', 1, true };
        }
        character = (character << 6) | static_cast<char32_t>(continuation & 0x3F);
    }

    if (character < minimum || !isValidUnicodeScalar(character)) {
        return Utf8DecodeResult { U'\uFFFD', 1, true };
    }

    return Utf8DecodeResult { character, length, true };
}

std::vector<char32_t> decodeUtf8(const std::string_view bytes) {
    std::vector<char32_t> result;
    result.reserve(bytes.size());

    std::size_t offset = 0;
    while (offset < bytes.size()) {
        Utf8DecodeResult decoded = decodeNextUtf8(bytes, offset);
        if (!decoded.complete) {
            decoded = Utf8DecodeResult { U'\uFFFD', 1, true };
        }
        result.push_back(decoded.character);
        offset += decoded.length;
    }

    return result;
}

std::string encodeUtf8(char32_t character) {
    if (!isValidUnicodeScalar(character)) {
        character = U'\uFFFD';
    }

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
    } else {
        result.push_back(static_cast<char>(0xF0 | ((character >> 18) & 0x07)));
        result.push_back(static_cast<char>(0x80 | ((character >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((character >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (character & 0x3F)));
    }
    return result;
}

char32_t sanitizeTerminalCell(const char32_t character) noexcept {
    if (
        !isValidUnicodeScalar(character) ||
        character < 0x20 ||
        (character >= 0x7F && character <= 0x9F)
    ) {
        return U'\uFFFD';
    }
    return character;
}

std::string sanitizeTerminalTitle(const std::string_view title) {
    std::string result;
    result.reserve(title.size());

    std::size_t offset = 0;
    while (offset < title.size()) {
        Utf8DecodeResult decoded = decodeNextUtf8(title, offset);
        if (!decoded.complete) {
            decoded = Utf8DecodeResult { U'\uFFFD', 1, true };
        }

        const char32_t character = decoded.character;
        if (character >= 0x20 && !(character >= 0x7F && character <= 0x9F)) {
            result += encodeUtf8(character);
        }
        offset += decoded.length;
    }

    return result;
}

} // namespace tt::detail
