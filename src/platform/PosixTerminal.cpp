/**
 * @file PosixTerminal.cpp
 * @brief Implements the Linux and macOS terminal backend.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "platform/PlatformTerminal.h"

#include "detail/EscapeSequenceParser.h"
#include "terminalTool/TerminalError.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

#include "terminalTool/TerminalSession.h"

namespace {

struct PosixState {
    tt::TerminalOptions options;
    termios originalAttributes {};
    int originalInputFlags = 0;
    bool attributesSaved = false;
    bool flagsSaved = false;
    bool initialized = false;
    bool focusReportingActive = false;
    bool alternateScreenActive = false;
    bool handlersInstalled = false;
    tt::detail::EscapeSequenceParser parser;
    std::array<struct sigaction, 5> oldActions {};
};

PosixState state;
volatile std::sig_atomic_t resizeRequested = 0;

constexpr std::array<int, 5> HANDLED_SIGNALS {
    SIGINT,
    SIGTERM,
    SIGHUP,
    SIGQUIT,
    SIGWINCH
};

void writeBestEffort(const char* bytes, const std::size_t size) noexcept {
    std::size_t writtenTotal = 0;
    while (writtenTotal < size) {
        const ssize_t written = ::write(
            STDOUT_FILENO,
            bytes + writtenTotal,
            size - writtenTotal
        );
        if (written <= 0) {
            return;
        }
        writtenTotal += static_cast<std::size_t>(written);
    }
}

void emergencyRestore() noexcept {
    static constexpr char BASE_RESTORE[] = "\033[0m\033[?25h\033[?7h\033[?1004l";
    writeBestEffort(BASE_RESTORE, sizeof(BASE_RESTORE) - 1);

    if (state.alternateScreenActive) {
        static constexpr char LEAVE_ALTERNATE[] = "\033[?1049l";
        writeBestEffort(LEAVE_ALTERNATE, sizeof(LEAVE_ALTERNATE) - 1);
    }

    if (state.attributesSaved) {
        (void) tcsetattr(STDIN_FILENO, TCSANOW, &state.originalAttributes);
    }
    if (state.flagsSaved) {
        (void) fcntl(STDIN_FILENO, F_SETFL, state.originalInputFlags);
    }
}

void signalHandler(const int signalNumber) {
    if (signalNumber == SIGWINCH) {
        resizeRequested = 1;
        return;
    }

    emergencyRestore();
    (void) std::signal(signalNumber, SIG_DFL);
    (void) ::raise(signalNumber);
}

[[noreturn]] void throwPosix(
    const tt::TerminalErrorCode code,
    const char* message
) {
    throw tt::TerminalError(code, message, static_cast<std::uint32_t>(errno));
}

void installHandlers() {
    for (std::size_t index = 0; index < HANDLED_SIGNALS.size(); index++) {
        struct sigaction action {};
        action.sa_handler = signalHandler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;

        if (sigaction(HANDLED_SIGNALS[index], &action, &state.oldActions[index]) != 0) {
            for (std::size_t restoreIndex = 0; restoreIndex < index; restoreIndex++) {
                (void) sigaction(
                    HANDLED_SIGNALS[restoreIndex],
                    &state.oldActions[restoreIndex],
                    nullptr
                );
            }
            throwPosix(
                tt::TerminalErrorCode::InstallSignalHandlerFailed,
                "terminalTool could not install POSIX signal handlers."
            );
        }
    }
    state.handlersInstalled = true;
}

void removeHandlers() noexcept {
    if (!state.handlersInstalled) {
        return;
    }

    for (std::size_t index = 0; index < HANDLED_SIGNALS.size(); index++) {
        (void) sigaction(HANDLED_SIGNALS[index], &state.oldActions[index], nullptr);
    }
    state.handlersInstalled = false;
}

std::string startupSequence(const tt::TerminalOptions& options) {
    std::string sequence;

    if (options.alternateScreen) {
        sequence += "\033[?1049h";
    }
    if (options.clearOnStart) {
        sequence += "\033[2J\033[H";
    }
    if (options.hideCursor) {
        sequence += "\033[?25l";
    }
    if (options.disableLineWrapping) {
        sequence += "\033[?7l";
    }
    if (options.enableFocusEvents) {
        sequence += "\033[?1004h";
    }
    if (!options.title.empty()) {
        sequence += "\033]0;" + options.title + "\007";
    }

    return sequence;
}

std::string shutdownSequence(const tt::TerminalOptions& options) {
    std::string sequence = "\033[0m";

    if (options.enableFocusEvents) {
        sequence += "\033[?1004l";
    }
    if (options.disableLineWrapping) {
        sequence += "\033[?7h";
    }
    if (options.hideCursor) {
        sequence += "\033[?25h";
    }
    if (options.clearOnExit) {
        sequence += "\033[2J\033[H";
    }
    if (options.alternateScreen) {
        sequence += "\033[?1049l";
    }

    return sequence;
}

} // namespace

namespace tt::detail {
    struct NativeInputEvent;

    void platformInitialize(const TerminalOptions& options) {
    if (::isatty(STDOUT_FILENO) == 0) {
        throw TerminalError(
            TerminalErrorCode::OutputIsNotTerminal,
            "terminalTool standard output is not an interactive POSIX terminal.",
            static_cast<std::uint32_t>(errno)
        );
    }
    if (::isatty(STDIN_FILENO) == 0) {
        throw TerminalError(
            TerminalErrorCode::InputIsNotTerminal,
            "terminalTool standard input is not an interactive POSIX terminal.",
            static_cast<std::uint32_t>(errno)
        );
    }

    state = PosixState {};
    state.options = options;

    if (tcgetattr(STDIN_FILENO, &state.originalAttributes) != 0) {
        throwPosix(
            TerminalErrorCode::ConfigureTerminalAttributesFailed,
            "terminalTool could not query POSIX terminal attributes."
        );
    }
    state.attributesSaved = true;

    state.originalInputFlags = fcntl(STDIN_FILENO, F_GETFL);
    if (state.originalInputFlags == -1) {
        throwPosix(
            TerminalErrorCode::ConfigureFileStatusFlagsFailed,
            "terminalTool could not query POSIX input descriptor flags."
        );
    }
    state.flagsSaved = true;

    termios raw = state.originalAttributes;
    raw.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
    raw.c_oflag &= static_cast<tcflag_t>(~OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN));
    // Keep ISIG enabled so Ctrl+C and other termination signals restore state.
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        throwPosix(
            TerminalErrorCode::ConfigureTerminalAttributesFailed,
            "terminalTool could not enable POSIX raw terminal input."
        );
    }

    if (fcntl(STDIN_FILENO, F_SETFL, state.originalInputFlags | O_NONBLOCK) == -1) {
        emergencyRestore();
        throwPosix(
            TerminalErrorCode::ConfigureFileStatusFlagsFailed,
            "terminalTool could not enable non-blocking POSIX input."
        );
    }

    if (options.installSignalHandlers) {
        try {
            installHandlers();
        } catch (...) {
            emergencyRestore();
            throw;
        }
    }

    state.focusReportingActive = options.enableFocusEvents;
    state.alternateScreenActive = options.alternateScreen;

    try {
        platformWriteOutput(startupSequence(options));
    } catch (...) {
        removeHandlers();
        emergencyRestore();
        throw;
    }

    state.initialized = true;
}

void platformShutdown() noexcept {
    if (!state.attributesSaved && !state.flagsSaved) {
        return;
    }

    if (state.initialized) {
        const std::string sequence = shutdownSequence(state.options);
        writeBestEffort(sequence.data(), sequence.size());
    } else {
        emergencyRestore();
    }

    removeHandlers();

    if (state.attributesSaved) {
        (void) tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.originalAttributes);
    }
    if (state.flagsSaved) {
        (void) fcntl(STDIN_FILENO, F_SETFL, state.originalInputFlags);
    }

    state = PosixState {};
    resizeRequested = 0;
}

TerminalDimensions platformTerminalSize() {
    winsize size {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_col == 0 || size.ws_row == 0) {
        throwPosix(
            TerminalErrorCode::QueryTerminalSizeFailed,
            "terminalTool could not query the visible POSIX terminal size."
        );
    }

    resizeRequested = 0;
    return TerminalDimensions {
        static_cast<int>(size.ws_col),
        static_cast<int>(size.ws_row)
    };
}

void platformWriteOutput(const std::string& output) {
    std::size_t writtenTotal = 0;

    while (writtenTotal < output.size()) {
        const ssize_t written = ::write(
            STDOUT_FILENO,
            output.data() + writtenTotal,
            output.size() - writtenTotal
        );

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            throwPosix(
                TerminalErrorCode::WriteOutputFailed,
                "terminalTool could not write terminal output."
            );
        }
        if (written == 0) {
            throw TerminalError(
                TerminalErrorCode::WriteOutputFailed,
                "terminalTool wrote zero bytes before completing terminal output."
            );
        }

        writtenTotal += static_cast<std::size_t>(written);
    }
}

void platformFlushInput() {
    if (tcflush(STDIN_FILENO, TCIFLUSH) != 0) {
        throwPosix(
            TerminalErrorCode::FlushInputFailed,
            "terminalTool could not flush pending POSIX input."
        );
    }
    state.parser.reset();
}

void platformReadInput(std::vector<NativeInputEvent>& events) {
    std::array<char, 4096> bytes {};

    while (true) {
        const ssize_t count = ::read(STDIN_FILENO, bytes.data(), bytes.size());

        if (count > 0) {
            state.parser.feed(bytes.data(), static_cast<std::size_t>(count), events);
            continue;
        }
        if (count == 0) {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }

        throwPosix(
            TerminalErrorCode::ReadInputFailed,
            "terminalTool could not read POSIX terminal input."
        );
    }

    state.parser.flushExpired(events);

    if (!state.options.enableFocusEvents) {
        events.erase(
            std::remove_if(
                events.begin(),
                events.end(),
                [](const NativeInputEvent& event) {
                    return
                        event.type == NativeInputEventType::FocusGained ||
                        event.type == NativeInputEventType::FocusLost;
                }
            ),
            events.end()
        );
    }
}

} // namespace tt::detail
