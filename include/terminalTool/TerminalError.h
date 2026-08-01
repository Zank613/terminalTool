/**
 * @file TerminalError.h
 * @brief Declares terminalTool initialization and terminal-operation errors.
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
    SessionAlreadyActive,       ///< A second TerminalSession was requested.
    InvalidOutputHandle,        ///< The standard output handle is invalid.
    InvalidInputHandle,         ///< The standard input handle is invalid.
    OutputIsNotTerminal,        ///< Standard output is not an interactive terminal.
    InputIsNotTerminal,         ///< Standard input is not an interactive terminal.
    QueryTerminalSizeFailed,    ///< The visible terminal dimensions could not be read.
    SetOutputCodePageFailed,    ///< The Windows output code page could not be changed.
    SetInputCodePageFailed,     ///< The Windows input code page could not be changed.
    ConfigureOutputModeFailed,  ///< ANSI output mode could not be enabled.
    ConfigureInputModeFailed,   ///< Console input mode could not be configured.
    InstallControlHandlerFailed,///< The Windows console control handler could not be installed.
    SetTitleFailed,             ///< The terminal title could not be changed.
    FrameBufferInitializationFailed ///< The terminal framebuffer could not be created.
};

/**
 * @brief Exception thrown when terminalTool cannot initialize or query the terminal.
 *
 * The exception stores both a portable TerminalErrorCode and an optional native
 * operating-system error value. On Windows, nativeErrorCode() normally contains
 * the value returned by GetLastError(). On POSIX systems it normally contains
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
