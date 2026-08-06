/**
 * @file Utf16.cpp
 * @brief Implements strict repeated UTF-16 input decoding helpers.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "detail/Utf16.h"

#include <algorithm>

namespace tt::detail {
namespace {

void appendReplacement(std::vector<char32_t>& output, const std::uint32_t count) {
    output.insert(output.end(), count, U'\uFFFD');
}

} // namespace

void flushRepeatedUtf16(Utf16RepeatState& state, std::vector<char32_t>& output) {
    appendReplacement(output, state.pendingHighSurrogateCount);
    resetRepeatedUtf16(state);
}

void resetRepeatedUtf16(Utf16RepeatState& state) noexcept {
    state.pendingHighSurrogate = 0;
    state.pendingHighSurrogateCount = 0;
}

void decodeRepeatedUtf16(
    Utf16RepeatState& state,
    const char16_t codeUnit,
    const std::uint16_t repeatCount,
    std::vector<char32_t>& output
) {
    if (repeatCount == 0) {
        return;
    }

    if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF) {
        if (
            state.pendingHighSurrogate != 0 &&
            state.pendingHighSurrogate != codeUnit
        ) {
            flushRepeatedUtf16(state, output);
        }

        state.pendingHighSurrogate = codeUnit;
        state.pendingHighSurrogateCount += repeatCount;
        return;
    }

    if (codeUnit >= 0xDC00 && codeUnit <= 0xDFFF) {
        const std::uint32_t pairCount = std::min<std::uint32_t>(
            state.pendingHighSurrogateCount,
            repeatCount
        );

        if (pairCount != 0) {
            const char32_t character = 0x10000 +
                ((static_cast<char32_t>(state.pendingHighSurrogate) - 0xD800) << 10) +
                (static_cast<char32_t>(codeUnit) - 0xDC00);
            output.insert(output.end(), pairCount, character);
            state.pendingHighSurrogateCount -= pairCount;
            if (state.pendingHighSurrogateCount == 0) {
                state.pendingHighSurrogate = 0;
            }
        }

        appendReplacement(output, static_cast<std::uint32_t>(repeatCount) - pairCount);
        return;
    }

    flushRepeatedUtf16(state, output);
    output.insert(output.end(), repeatCount, static_cast<char32_t>(codeUnit));
}

} // namespace tt::detail
