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

#include "NativeInputEvent.h"

namespace tt::detail {

/**
 * @brief Incrementally decodes UTF-8, control bytes, CSI, and SS3 sequences.
 *
 * The parser accepts arbitrarily split reads. A lone Escape byte is delayed
 * briefly so it can still become the prefix of a later sequence.
 */
class EscapeSequenceParser {
private:
    std::string buffer;
    std::chrono::steady_clock::time_point escapePendingSince {};
    bool escapePending = false;

    void parseAvailable(std::vector<NativeInputEvent>& events, bool forceEscape);

public:
    /** @brief Adds bytes and emits every complete event currently available. */
    void feed(const char* bytes, std::size_t size, std::vector<NativeInputEvent>& events);

    /** @brief Adds a string of bytes and emits complete events. */
    void feed(const std::string& bytes, std::vector<NativeInputEvent>& events);

    /** @brief Emits a pending lone Escape once its ambiguity timeout expires. */
    void flushExpired(std::vector<NativeInputEvent>& events);

    /** @brief Forces all pending input, including a lone Escape, to be emitted. */
    void flush(std::vector<NativeInputEvent>& events);

    /** @brief Clears all pending partial input. */
    void reset() noexcept;
};

} // namespace tt::detail
