/**
 * @file EscapeSequenceParser.h
 * @brief Incremental parser for POSIX terminal keyboard and focus sequences.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "detail/NativeInputEvent.h"

namespace tt::detail {

/**
 * @brief Incrementally decodes UTF-8, control bytes, CSI, and SS3 sequences.
 *
 * Reads may be split at any byte boundary. A lone Escape byte is delayed
 * briefly so it can become a later CSI, SS3, or Alt-modified sequence.
 * Malformed or overlong escape sequences are bounded and recovered instead of
 * remaining buffered forever.
 */
class EscapeSequenceParser {
private:
    static constexpr std::size_t MAX_ESCAPE_SEQUENCE_LENGTH = 128;

    std::string buffer;
    std::chrono::steady_clock::time_point escapePendingSince {};
    bool escapePending = false;

    void parseAvailable(std::vector<NativeInputEvent>& events, bool forceEscape);

public:
    /** @brief Maximum accepted bytes in one pending escape sequence. */
    [[nodiscard]] static constexpr std::size_t maximumEscapeSequenceLength() noexcept {
        return MAX_ESCAPE_SEQUENCE_LENGTH;
    }

    /** @brief Adds bytes and emits every complete event currently available. */
    void feed(const char* bytes, std::size_t size, std::vector<NativeInputEvent>& events);

    /** @brief Adds a string of bytes and emits complete events. */
    void feed(const std::string& bytes, std::vector<NativeInputEvent>& events);

    /** @brief Emits a pending lone Escape once its ambiguity timeout expires. */
    void flushExpired(std::vector<NativeInputEvent>& events);

    /** @brief Forces all pending input, including malformed partial input, out. */
    void flush(std::vector<NativeInputEvent>& events);

    /** @brief Clears all pending partial input. */
    void reset() noexcept;

    /** @return Number of currently buffered undecoded bytes. */
    [[nodiscard]] std::size_t bufferedByteCount() const noexcept { return buffer.size(); }
};

} // namespace tt::detail
