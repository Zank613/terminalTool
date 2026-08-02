/**
 * @file TerminalError.h
 * @brief Declares terminalTool initialization and runtime terminal errors.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace tt {

/**
 * @brief Identifies the operation that caused a TerminalError.
 */
enum class TerminalErrorCode {
    SessionAlreadyActive,        ///< A second TerminalSession was requested.
    InvalidOutputHandle,         ///< The standard output handle is invalid.
    InvalidInputHandle,          ///< The standard input handle is invalid.
    OutputIsNotTerminal,         ///< Standard output is not an interactive terminal.
    InputIsNotTerminal,          ///< Standard input is not an interactive terminal.
    QueryTerminalSizeFailed,     ///< The visible terminal dimensions could not be read.
    QueryTitleFailed,            ///< The original Windows terminal title could not be read.
    SetOutputCodePageFailed,     ///< The Windows output code page could not be changed.
    SetInputCodePageFailed,      ///< The Windows input code page could not be changed.
    ConfigureOutputModeFailed,   ///< ANSI output mode could not be enabled.
    ConfigureInputModeFailed,    ///< Console input mode could not be configured.
    InstallControlHandlerFailed, ///< The Windows console control handler could not be installed.
    SetTitleFailed,              ///< The terminal title could not be changed.
    FrameBufferInitializationFailed, ///< The initial terminal framebuffer could not be created.
    FrameBufferResizeFailed,     ///< A resized terminal framebuffer could not be created.
    FlushInputFailed,            ///< Pending console input could not be discarded.
    QueryInputEventCountFailed,  ///< The number of pending input events could not be queried.
    ReadInputFailed,             ///< Console input events could not be read.
    WriteOutputFailed            ///< Terminal output could not be written or flushed.
};

/**
 * @brief Exception thrown when terminalTool cannot complete a terminal operation.
 *
 * The exception stores both a portable TerminalErrorCode and an optional native
 * operating-system error value. On Windows, nativeErrorCode() normally contains
 * a value returned by GetLastError(). On POSIX systems it normally contains
 * errno. A value of zero means that no native code was available.
 */
class TerminalError : public std::runtime_error {
private:
    TerminalErrorCode errorCode;
    std::uint32_t operatingSystemErrorCode;

public:
    /**
     * @brief Creates a terminal exception.
     * @param code Portable terminalTool error category.
     * @param message Human-readable explanation of the failure.
     * @param nativeErrorCode Optional operating-system error value.
     */
    TerminalError(TerminalErrorCode code, const std::string& message, std::uint32_t nativeErrorCode = 0);

    /** @return The portable terminalTool error category. */
    [[nodiscard]] TerminalErrorCode code() const noexcept;

    /** @return The native operating-system error value, or zero when unavailable. */
    [[nodiscard]] std::uint32_t nativeErrorCode() const noexcept;
};

} // namespace tt
