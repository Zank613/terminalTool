/**
 * @file TerminalError.cpp
 * @brief Implements tt::TerminalError.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/TerminalError.h"

namespace tt {

TerminalError::TerminalError(
    const TerminalErrorCode code,
    const std::string& message,
    const std::uint32_t nativeErrorCode
) : std::runtime_error(message),
    errorCode(code),
    operatingSystemErrorCode(nativeErrorCode) {}

TerminalErrorCode TerminalError::code() const noexcept {
    return errorCode;
}

std::uint32_t TerminalError::nativeErrorCode() const noexcept {
    return operatingSystemErrorCode;
}

} // namespace tt
