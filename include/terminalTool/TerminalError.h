/**
 * @file TerminalError.h
 * @brief Declares terminalTool initialization and runtime errors.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace tt {

/** @brief Portable category identifying a failed terminal operation. */
enum class TerminalErrorCode {
    SessionAlreadyActive,
    InvalidOutputHandle,
    InvalidInputHandle,
    OutputIsNotTerminal,
    InputIsNotTerminal,
    QueryTerminalSizeFailed,
    QueryTitleFailed,
    SetOutputCodePageFailed,
    SetInputCodePageFailed,
    ConfigureOutputModeFailed,
    ConfigureInputModeFailed,
    ConfigureTerminalAttributesFailed,
    ConfigureFileStatusFlagsFailed,
    InstallControlHandlerFailed,
    InstallSignalHandlerFailed,
    SetTitleFailed,
    FrameBufferInitializationFailed,
    FrameBufferResizeFailed,
    FlushInputFailed,
    QueryInputEventCountFailed,
    ReadInputFailed,
    WriteOutputFailed
};

/**
 * @brief Exception thrown when terminalTool cannot complete an operation.
 *
 * nativeErrorCode() contains GetLastError() on Windows or errno on POSIX when
 * the platform supplied one. Zero means no native value was available.
 */
class TerminalError : public std::runtime_error {
private:
    TerminalErrorCode errorCode;
    std::uint32_t operatingSystemErrorCode;

public:
    /** @brief Constructs a documented terminal failure. */
    TerminalError(TerminalErrorCode code, const std::string& message, std::uint32_t nativeErrorCode = 0);

    /** @return Portable terminalTool error category. */
    [[nodiscard]] TerminalErrorCode code() const noexcept;

    /** @return Native operating-system error value, or zero. */
    [[nodiscard]] std::uint32_t nativeErrorCode() const noexcept;
};

} // namespace tt
