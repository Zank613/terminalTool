/**
 * @file Utf16.h
 * @brief Declares strict repeated UTF-16 input decoding helpers.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <cstdint>
#include <vector>

namespace tt::detail {

/** @brief Pending high-surrogate state retained between native input records. */
struct Utf16RepeatState {
    char16_t pendingHighSurrogate = 0;
    std::uint32_t pendingHighSurrogateCount = 0;
};

/**
 * @brief Decodes one UTF-16 code unit repeated by a native input record.
 *
 * Repeated high and low surrogates are paired one-for-one. Unpaired surrogate
 * code units become U+FFFD. Output is appended to @p output.
 */
void decodeRepeatedUtf16(
    Utf16RepeatState& state,
    char16_t codeUnit,
    std::uint16_t repeatCount,
    std::vector<char32_t>& output
);

/** @brief Emits U+FFFD for pending high surrogates and clears the state. */
void flushRepeatedUtf16(Utf16RepeatState& state, std::vector<char32_t>& output);

/** @brief Discards pending surrogate state without producing output. */
void resetRepeatedUtf16(Utf16RepeatState& state) noexcept;

} // namespace tt::detail
