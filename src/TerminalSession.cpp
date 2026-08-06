/**
 * @file TerminalSession.cpp
 * @brief Implements single-session RAII ownership over the platform backend.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/TerminalSession.h"

#include "platform/PlatformTerminal.h"
#include "terminalTool/Console.h"
#include "terminalTool/Input.h"
#include "terminalTool/TerminalError.h"

#include <atomic>
#include <memory>
#include <utility>

namespace {

std::atomic<tt::TerminalSession*> activeSession { nullptr };

} // namespace

namespace tt {

class TerminalSession::Implementation {
public:
    TerminalOptions sessionOptions;
    bool active = false;

    explicit Implementation(TerminalOptions options)
        : sessionOptions(std::move(options)) {}
};

TerminalSession::TerminalSession(const std::string& title)
    : TerminalSession([&title] {
        TerminalOptions options;
        options.title = title;
        return options;
    }()) {}

TerminalSession::TerminalSession(const TerminalOptions& options) {
    TerminalSession* expected = nullptr;
    if (!activeSession.compare_exchange_strong(expected, this)) {
        throw TerminalError(
            TerminalErrorCode::SessionAlreadyActive,
            "terminalTool permits only one TerminalSession at a time."
        );
    }

    try {
        implementation = std::make_unique<Implementation>(options);
        detail::platformInitialize(options);

        const detail::TerminalDimensions size = detail::platformTerminalSize();
        Console::initializeFrameBuffer(size.width, size.height);
        Input::initialize(options.maximumQueuedInputEvents);
        implementation->active = true;
    } catch (...) {
        Input::reset();
        Console::shutdownFrameBuffer();
        detail::platformShutdown();
        implementation.reset();
        activeSession.store(nullptr);
        throw;
    }
}

TerminalSession::~TerminalSession() {
    if (implementation != nullptr) {
        Input::reset();
        Console::shutdownFrameBuffer();
        detail::platformShutdown();
        implementation->active = false;
        implementation.reset();
    }

    TerminalSession* expected = this;
    activeSession.compare_exchange_strong(expected, nullptr);
}

bool TerminalSession::update() {
    if (!isActive()) {
        throw TerminalError(
            TerminalErrorCode::NoActiveSession,
            "tt::TerminalSession::update() requires an active session."
        );
    }

    const detail::PlatformUpdateResult platformResult = detail::platformUpdate();
    if (platformResult.invalidateFrame) {
        Console::invalidate();
    }

    if (!platformResult.checkResize) {
        return false;
    }
    return Console::resizeToTerminal();
}

bool TerminalSession::isActive() const noexcept {
    return implementation != nullptr && implementation->active;
}

bool TerminalSession::hasActiveSession() noexcept {
    return activeSession.load() != nullptr;
}

const TerminalOptions& TerminalSession::options() const noexcept {
    static const TerminalOptions inactiveOptions {};
    return implementation != nullptr ? implementation->sessionOptions : inactiveOptions;
}

} // namespace tt
