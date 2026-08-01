/**
 * @file TerminalSession.h
 * @brief Declares the single-instance RAII terminal lifetime manager.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <atomic>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tt {

/**
 * @brief Configures and restores the process's one interactive terminal session.
 *
 * Exactly one TerminalSession may exist at a time. Constructing another while
 * one is active throws TerminalError with TerminalErrorCode::SessionAlreadyActive.
 * Successful construction initializes Console and Input. Destruction restores
 * cursor visibility, line wrapping, terminal modes, and Windows code pages.
 */
class TerminalSession {
private:
    bool active = false;
    static std::atomic<TerminalSession*> activeSession;

#ifdef _WIN32
    HANDLE outputHandle = INVALID_HANDLE_VALUE;
    HANDLE inputHandle = INVALID_HANDLE_VALUE;
    DWORD originalOutputMode = 0;
    DWORD originalInputMode = 0;
    UINT originalOutputCodePage = 0;
    UINT originalInputCodePage = 0;
    bool outputModeSaved = false;
    bool inputModeSaved = false;
    bool outputCodePageChanged = false;
    bool inputCodePageChanged = false;
    bool controlHandlerInstalled = false;

    static BOOL WINAPI controlHandler(DWORD eventType);
    void emergencyRestore() const noexcept;
#endif

    void configure(const std::string& title);
    void restore() noexcept;
    void restoreSystemState() noexcept;

public:
    /**
     * @brief Starts the process's single interactive terminal session.
     * @param title Window title used on Windows. UTF-8 is accepted.
     * @throws TerminalError when another session exists or terminal setup fails.
     */
    explicit TerminalSession(const std::string& title = "terminalTool");

    /** @brief Restores the original terminal state. */
    ~TerminalSession();

    /** @brief Terminal sessions cannot be copied. */
    TerminalSession(const TerminalSession&) = delete;

    /** @brief Terminal sessions cannot be copy-assigned. */
    TerminalSession& operator=(const TerminalSession&) = delete;

    /** @brief Terminal sessions cannot be moved. */
    TerminalSession(TerminalSession&&) = delete;

    /** @brief Terminal sessions cannot be move-assigned. */
    TerminalSession& operator=(TerminalSession&&) = delete;

    /**
     * @brief Checks for terminal resizing.
     * @return `true` when the framebuffer dimensions changed.
     * @throws TerminalError when the visible terminal size cannot be queried.
     */
    [[nodiscard]] bool update();

    /** @return `true` while this terminal session is fully configured. */
    [[nodiscard]] bool isActive() const noexcept;

    /** @return `true` when any TerminalSession currently owns the terminal. */
    [[nodiscard]] static bool hasActiveSession() noexcept;
};

} // namespace tt
