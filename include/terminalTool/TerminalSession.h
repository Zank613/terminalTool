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
 * @brief Options applied when a TerminalSession is created.
 */
struct TerminalOptions {
    /** @brief UTF-8 terminal title. An empty string leaves the title unchanged. */
    std::string title = "terminalTool";

    /**
     * @brief Use the terminal's alternate screen buffer when available.
     *
     * When enabled, the original terminal contents reappear when the session
     * ends. Disable this when the application's output should remain visible
     * in the normal terminal history after exit.
     */
    bool alternateScreen = true;
};

/**
 * @brief Configures and restores the process's one interactive terminal session.
 *
 * Exactly one TerminalSession may exist at a time. Constructing another while
 * one is active throws TerminalError with TerminalErrorCode::SessionAlreadyActive.
 * Successful construction initializes Console and Input. Destruction restores
 * cursor visibility, line wrapping, terminal modes, the original Windows title,
 * Windows code pages, and the normal screen buffer when alternateScreen is used.
 *
 * All TerminalSession, Console, and Input operations are intended to be called
 * from the thread that owns the terminal.
 */
class TerminalSession {
private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;

public:
    /**
     * @brief Starts the process's single interactive terminal session.
     * @param title UTF-8 title. The alternate screen buffer is enabled.
     * @throws TerminalError when another session exists or terminal setup fails.
     */
    explicit TerminalSession(const std::string& title = "terminalTool");

    /**
     * @brief Starts a terminal session with explicit options.
     * @param options Session configuration.
     * @throws TerminalError when another session exists or terminal setup fails.
     */
    explicit TerminalSession(const TerminalOptions& options);

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
     * @throws TerminalError when the terminal size cannot be queried or the
     *         resized framebuffer cannot be allocated.
     */
    [[nodiscard]] bool update();

    /** @return `true` while this terminal session is fully configured. */
    [[nodiscard]] bool isActive() const noexcept;

    /** @return `true` when any TerminalSession currently owns the terminal. */
    [[nodiscard]] static bool hasActiveSession() noexcept;
};

} // namespace tt
