/**
 * @file TerminalSession.h
 * @brief Declares the single-instance RAII terminal lifetime manager.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <memory>
#include <string>

namespace tt {

/**
 * @brief User-visible terminal behaviour configured for a session.
 *
 * Defaults are suitable for a full-screen terminal game. Every enabled mode is
 * restored on normal destruction. Windows control events and POSIX termination
 * signals receive a best-effort emergency restoration path.
 */
struct TerminalOptions {
    /** @brief UTF-8 terminal title. Empty leaves the title unchanged. */
    std::string title = "terminalTool";

    /** @brief Use the alternate screen so previous terminal contents return. */
    bool alternateScreen = true;

    /** @brief Hide the terminal cursor for the duration of the session. */
    bool hideCursor = true;

    /** @brief Disable automatic line wrapping while the session is active. */
    bool disableLineWrapping = true;

    /**
     * @brief Enable terminal focus reporting where supported.
     *
     * Windows console focus events are always consumed when delivered. POSIX
     * terminals are explicitly asked to send CSI I and CSI O sequences.
     */
    bool enableFocusEvents = true;

    /** @brief Clear the active screen when the session starts. */
    bool clearOnStart = true;

    /** @brief Clear the active screen immediately before session restoration. */
    bool clearOnExit = false;

    /**
     * @brief Install emergency restoration handlers.
     *
     * On POSIX this covers SIGINT, SIGTERM, SIGHUP, and SIGQUIT, while SIGWINCH
     * is observed for resizing. On Windows a console control handler is used.
     */
    bool installSignalHandlers = true;

    /**
     * @brief Change and later restore the Windows console title.
     *
     * POSIX terminal protocols cannot reliably query the previous title, so on
     * POSIX the requested title is set but cannot be restored exactly.
     */
    bool restoreTitle = true;
};

/**
 * @brief Configures and restores the process's one interactive terminal.
 *
 * Exactly one TerminalSession may exist at a time. Successful construction
 * initializes the internal platform backend, Console framebuffer, and Input
 * state. Destruction reverses every configured mode without throwing.
 *
 * All terminalTool calls are intended for the terminal-owning thread.
 */
class TerminalSession {
private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;

public:
    /**
     * @brief Starts a session using default game-oriented options.
     * @param title UTF-8 title.
     * @throws TerminalError when another session exists or setup fails.
     */
    explicit TerminalSession(const std::string& title = "terminalTool");

    /**
     * @brief Starts a session with explicit behaviour.
     * @param options Session configuration copied into the session.
     * @throws TerminalError when another session exists or setup fails.
     */
    explicit TerminalSession(const TerminalOptions& options);

    /** @brief Restores terminal state and releases global ownership. */
    ~TerminalSession();

    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;
    TerminalSession(TerminalSession&&) = delete;
    TerminalSession& operator=(TerminalSession&&) = delete;

    /**
     * @brief Checks terminal dimensions and resizes the framebuffer if needed.
     * @return true when framebuffer dimensions changed.
     * @throws TerminalError when size querying or allocation fails.
     */
    [[nodiscard]] bool update();

    /** @return Whether this session completed initialization. */
    [[nodiscard]] bool isActive() const noexcept;

    /** @return Whether any TerminalSession currently owns the process terminal. */
    [[nodiscard]] static bool hasActiveSession() noexcept;

    /** @return The immutable options used to construct this session. */
    [[nodiscard]] const TerminalOptions& options() const noexcept;
};

} // namespace tt
