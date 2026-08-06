/**
 * @file Unicode.h
 * @brief Internal strict Unicode scalar and UTF-8 helpers.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tt::detail {

/** @brief Result from decoding one UTF-8 scalar. */
struct Utf8DecodeResult {
    char32_t character = U'\uFFFD';
    std::size_t length = 0;
    bool complete = false;
};

/** @return Whether character is a valid Unicode scalar value. */
[[nodiscard]] bool isValidUnicodeScalar(char32_t character) noexcept;

/**
 * @brief Decodes one strict UTF-8 scalar at offset.
 * @return A complete replacement scalar consuming one byte for malformed input,
 * or `complete == false` and `length == 0` for an incomplete trailing sequence.
 */
[[nodiscard]] Utf8DecodeResult decodeNextUtf8(std::string_view bytes, std::size_t offset) noexcept;

/** @brief Decodes all UTF-8, replacing malformed input with U+FFFD. */
[[nodiscard]] std::vector<char32_t> decodeUtf8(std::string_view bytes);

/** @brief Encodes one scalar, replacing invalid values with U+FFFD. */
[[nodiscard]] std::string encodeUtf8(char32_t character);

/**
 * @brief Replaces terminal control scalars with U+FFFD.
 *
 * C0, DEL, C1, surrogate, and out-of-range values are unsafe as framebuffer
 * cells because they can alter terminal state instead of occupying one cell.
 */
[[nodiscard]] char32_t sanitizeTerminalCell(char32_t character) noexcept;

/**
 * @brief Returns UTF-8 safe for an OSC terminal-title sequence.
 *
 * Control scalars are removed so a title cannot terminate or inject another
 * terminal control sequence.
 */
[[nodiscard]] std::string sanitizeTerminalTitle(std::string_view title);

} // namespace tt::detail
