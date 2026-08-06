/**
 * @file PosixTerminal.cpp
 * @brief Implements the Linux and macOS terminal backend.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "platform/PlatformTerminal.h"

#include "detail/EscapeSequenceParser.h"
#include "detail/Unicode.h"
#include "terminalTool/TerminalError.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr std::array<int, 7> HANDLED_SIGNALS {
    SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGWINCH, SIGTSTP, SIGCONT
};

struct PosixState {
    tt::TerminalOptions options;
    termios originalAttributes {};
    int originalInputFlags = 0;
    bool attributesSaved = false;
    bool flagsSaved = false;
    bool initialized = false;
    bool handlersInstalled = false;
    tt::detail::EscapeSequenceParser parser;
    std::array<struct sigaction, HANDLED_SIGNALS.size()> oldActions {};
    std::array<char, 128> emergencySequence {};
    std::size_t emergencySequenceLength = 0;
};

PosixState state;
volatile std::sig_atomic_t resizeRequested = 0;
volatile std::sig_atomic_t resumeRequested = 0;
volatile std::sig_atomic_t emergencyRestored = 0;

[[nodiscard]] int signalIndex(const int signalNumber) noexcept {
    for (std::size_t index = 0; index < HANDLED_SIGNALS.size(); index++) {
        if (HANDLED_SIGNALS[index] == signalNumber) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void writeBestEffort(const char* bytes, const std::size_t size) noexcept {
    std::size_t writtenTotal = 0;
    while (writtenTotal < size) {
        const ssize_t written = ::write(STDOUT_FILENO, bytes + writtenTotal, size - writtenTotal);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return;
        }
        writtenTotal += static_cast<std::size_t>(written);
    }
}

std::string startupSequence(const tt::TerminalOptions& options) {
    std::string sequence;
    if (options.alternateScreen) sequence += "\033[?1049h";
    if (options.clearOnStart) sequence += "\033[2J\033[H";
    if (options.hideCursor) sequence += "\033[?25l";
    if (options.disableLineWrapping) sequence += "\033[?7l";
    if (options.enableFocusEvents) sequence += "\033[?1004h";
    if (!options.title.empty()) {
        sequence += "\033]0;" + tt::detail::sanitizeTerminalTitle(options.title) + "\007";
    }
    return sequence;
}

std::string shutdownSequence(const tt::TerminalOptions& options) {
    std::string sequence = "\033[0m";
    if (options.enableFocusEvents) sequence += "\033[?1004l";
    if (options.disableLineWrapping) sequence += "\033[?7h";
    if (options.hideCursor) sequence += "\033[?25h";
    if (options.clearOnExit) sequence += "\033[2J\033[H";
    if (options.alternateScreen) sequence += "\033[?1049l";
    return sequence;
}

void prepareEmergencySequence() {
    const std::string sequence = shutdownSequence(state.options);
    state.emergencySequenceLength = std::min(sequence.size(), state.emergencySequence.size());
    std::copy_n(sequence.data(), state.emergencySequenceLength, state.emergencySequence.data());
}

void emergencyRestore() noexcept {
    if (emergencyRestored != 0) {
        return;
    }
    emergencyRestored = 1;

    if (state.emergencySequenceLength != 0) {
        writeBestEffort(state.emergencySequence.data(), state.emergencySequenceLength);
    }
    if (state.attributesSaved) {
        (void) tcsetattr(STDIN_FILENO, TCSANOW, &state.originalAttributes);
    }
    if (state.flagsSaved) {
        (void) fcntl(STDIN_FILENO, F_SETFL, state.originalInputFlags);
    }
}

void reinstallCurrentHandler(const int signalNumber) noexcept;

void dispatchPrevious(const int signalNumber) noexcept {
    const int index = signalIndex(signalNumber);
    if (index < 0) {
        return;
    }

    const struct sigaction& previous = state.oldActions[static_cast<std::size_t>(index)];
    if (previous.sa_handler == SIG_IGN) {
        return;
    }

    (void) sigaction(signalNumber, &previous, nullptr);

    sigset_t signalSet {};
    sigset_t previousMask {};
    sigemptyset(&signalSet);
    sigaddset(&signalSet, signalNumber);
    (void) sigprocmask(SIG_UNBLOCK, &signalSet, &previousMask);
    (void) ::kill(::getpid(), signalNumber);
    (void) sigprocmask(SIG_SETMASK, &previousMask, nullptr);

    reinstallCurrentHandler(signalNumber);
}

void signalHandler(const int signalNumber) {
    if (signalNumber == SIGWINCH) {
        resizeRequested = 1;
        dispatchPrevious(signalNumber);
        return;
    }

    if (signalNumber == SIGCONT) {
        resumeRequested = 1;
        resizeRequested = 1;
        dispatchPrevious(signalNumber);
        return;
    }

    emergencyRestore();

    if (signalNumber == SIGTSTP) {
        const int index = signalIndex(signalNumber);
        if (
            index >= 0 &&
            state.oldActions[static_cast<std::size_t>(index)].sa_handler == SIG_DFL
        ) {
            // SIGTSTP is ignored for orphaned process groups. SIGSTOP preserves
            // the expected suspend/resume contract after terminal restoration.
            (void) ::kill(::getpid(), SIGSTOP);
        } else {
            dispatchPrevious(signalNumber);
        }
        resumeRequested = 1;
        resizeRequested = 1;
        return;
    }

    dispatchPrevious(signalNumber);
    resumeRequested = 1;
    resizeRequested = 1;
}

void reinstallCurrentHandler(const int signalNumber) noexcept {
    struct sigaction action {};
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    (void) sigaction(signalNumber, &action, nullptr);
}

[[noreturn]] void throwPosix(const tt::TerminalErrorCode code, const char* message) {
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
                (void) sigaction(HANDLED_SIGNALS[restoreIndex], &state.oldActions[restoreIndex], nullptr);
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

termios rawAttributes() {
    termios raw = state.originalAttributes;
    raw.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
    raw.c_oflag &= static_cast<tcflag_t>(~OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN));
    // Keep ISIG enabled for Ctrl+C and Ctrl+Z restoration.
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    return raw;
}

void applyRuntimeState(const bool flushInput) {
    const termios raw = rawAttributes();
    if (tcsetattr(STDIN_FILENO, flushInput ? TCSAFLUSH : TCSANOW, &raw) != 0) {
        throwPosix(
            tt::TerminalErrorCode::ConfigureTerminalAttributesFailed,
            "terminalTool could not enable POSIX raw terminal input."
        );
    }
    if (fcntl(STDIN_FILENO, F_SETFL, state.originalInputFlags | O_NONBLOCK) == -1) {
        throwPosix(
            tt::TerminalErrorCode::ConfigureFileStatusFlagsFailed,
            "terminalTool could not enable non-blocking POSIX input."
        );
    }
}

} // namespace

namespace tt::detail {

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
    resizeRequested = 0;
    resumeRequested = 0;
    emergencyRestored = 0;

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
    prepareEmergencySequence();

    try {
        applyRuntimeState(true);
        if (options.installSignalHandlers) {
            installHandlers();
        }
        platformWriteOutput(startupSequence(options));
        state.initialized = true;
    } catch (...) {
        emergencyRestore();
        removeHandlers();
        throw;
    }
}

void platformShutdown() noexcept {
    if (!state.attributesSaved && !state.flagsSaved) {
        return;
    }

    removeHandlers();
    emergencyRestore();
    state = PosixState {};
    resizeRequested = 0;
    resumeRequested = 0;
    emergencyRestored = 0;
}

bool platformIsInitialized() noexcept {
    return state.initialized;
}

PlatformUpdateResult platformUpdate() {
    PlatformUpdateResult result;
    result.checkResize = !state.handlersInstalled || resizeRequested != 0;

    if (resumeRequested != 0) {
        resumeRequested = 0;
        applyRuntimeState(false);
        platformWriteOutput(startupSequence(state.options));
        emergencyRestored = 0;
        state.initialized = true;
        result.checkResize = true;
        result.invalidateFrame = true;
    }

    return result;
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
            if (errno == EINTR) continue;
            throwPosix(TerminalErrorCode::WriteOutputFailed, "terminalTool could not write terminal output.");
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
        throwPosix(TerminalErrorCode::FlushInputFailed, "terminalTool could not flush pending POSIX input.");
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
        if (count == 0 || errno == EAGAIN || errno == EWOULDBLOCK) break;
        if (errno == EINTR) continue;
        throwPosix(TerminalErrorCode::ReadInputFailed, "terminalTool could not read POSIX terminal input.");
    }

    state.parser.flushExpired(events);
    if (!state.options.enableFocusEvents) {
        events.erase(
            std::remove_if(events.begin(), events.end(), [](const NativeInputEvent& event) {
                return event.type == NativeInputEventType::FocusGained ||
                       event.type == NativeInputEventType::FocusLost;
            }),
            events.end()
        );
    }
}

} // namespace tt::detail
