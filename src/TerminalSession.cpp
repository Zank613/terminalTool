/**
 * @file TerminalSession.cpp
 * @brief Implements tt::TerminalSession setup, ownership, and restoration.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/TerminalSession.h"

#include "terminalTool/Colour.h"
#include "terminalTool/Console.h"
#include "terminalTool/Input.h"
#include "terminalTool/TerminalError.h"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vector>
#else
#include <unistd.h>
#endif

namespace {

std::atomic<tt::TerminalSession*> activeSession { nullptr };

void writeNoThrow(const std::string& sequence) noexcept {
    if (sequence.empty()) {
        return;
    }

    try {
        std::cout.write(sequence.data(), static_cast<std::streamsize>(sequence.size()));
        std::cout.flush();
    } catch (...) {
        // Destruction and emergency restoration must never throw.
    }
}

#ifdef _WIN32

struct WindowsState {
    HANDLE outputHandle = INVALID_HANDLE_VALUE;
    HANDLE inputHandle = INVALID_HANDLE_VALUE;
    DWORD originalOutputMode = 0;
    DWORD originalInputMode = 0;
    UINT originalOutputCodePage = 0;
    UINT originalInputCodePage = 0;
    std::wstring originalTitle;
    bool outputModeSaved = false;
    bool inputModeSaved = false;
    bool outputCodePageChanged = false;
    bool inputCodePageChanged = false;
    bool titleSaved = false;
    bool titleChanged = false;
    bool controlHandlerInstalled = false;
    bool alternateScreenActive = false;
};

std::atomic<WindowsState*> activeWindowsState { nullptr };

[[noreturn]] void throwWindowsError(
    const tt::TerminalErrorCode code,
    const std::string& message
) {
    throw tt::TerminalError(code, message, static_cast<std::uint32_t>(GetLastError()));
}

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int requiredCharacters = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0
    );

    if (requiredCharacters <= 0) {
        throwWindowsError(
            tt::TerminalErrorCode::SetTitleFailed,
            "terminalTool could not decode the UTF-8 terminal title."
        );
    }

    std::wstring result(static_cast<std::size_t>(requiredCharacters), L'\0');

    if (MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        requiredCharacters
    ) <= 0) {
        throwWindowsError(
            tt::TerminalErrorCode::SetTitleFailed,
            "terminalTool could not decode the UTF-8 terminal title."
        );
    }

    return result;
}

void restoreWindowsState(WindowsState& state) noexcept {
    if (state.titleChanged && state.titleSaved) {
        SetConsoleTitleW(state.originalTitle.c_str());
        state.titleChanged = false;
    }

    if (state.outputModeSaved && state.outputHandle != INVALID_HANDLE_VALUE && state.outputHandle != nullptr) {
        SetConsoleMode(state.outputHandle, state.originalOutputMode);
    }

    if (state.inputModeSaved && state.inputHandle != INVALID_HANDLE_VALUE && state.inputHandle != nullptr) {
        SetConsoleMode(state.inputHandle, state.originalInputMode);
    }

    if (state.outputCodePageChanged && state.originalOutputCodePage != 0) {
        SetConsoleOutputCP(state.originalOutputCodePage);
        state.outputCodePageChanged = false;
    }

    if (state.inputCodePageChanged && state.originalInputCodePage != 0) {
        SetConsoleCP(state.originalInputCodePage);
        state.inputCodePageChanged = false;
    }
}

BOOL WINAPI terminalControlHandler(DWORD eventType);

void removeControlHandler(WindowsState& state) noexcept {
    if (!state.controlHandlerInstalled) {
        return;
    }

    SetConsoleCtrlHandler(&terminalControlHandler, FALSE);
    state.controlHandlerInstalled = false;

    WindowsState* expected = &state;
    activeWindowsState.compare_exchange_strong(expected, nullptr);
}

void emergencyRestoreWindows(WindowsState& state) noexcept {
    if (state.outputModeSaved && state.outputHandle != INVALID_HANDLE_VALUE && state.outputHandle != nullptr) {
        std::string sequence = "\033[0m\033[?7h\033[?25h";

        if (state.alternateScreenActive) {
            sequence += "\033[?1049l";
        }

        DWORD bytesWritten = 0;
        WriteFile(
            state.outputHandle,
            sequence.data(),
            static_cast<DWORD>(sequence.size()),
            &bytesWritten,
            nullptr
        );
    }

    if (state.titleChanged && state.titleSaved) {
        SetConsoleTitleW(state.originalTitle.c_str());
    }

    if (state.outputModeSaved && state.outputHandle != INVALID_HANDLE_VALUE && state.outputHandle != nullptr) {
        SetConsoleMode(state.outputHandle, state.originalOutputMode);
    }

    if (state.inputModeSaved && state.inputHandle != INVALID_HANDLE_VALUE && state.inputHandle != nullptr) {
        SetConsoleMode(state.inputHandle, state.originalInputMode);
    }

    if (state.outputCodePageChanged && state.originalOutputCodePage != 0) {
        SetConsoleOutputCP(state.originalOutputCodePage);
    }

    if (state.inputCodePageChanged && state.originalInputCodePage != 0) {
        SetConsoleCP(state.originalInputCodePage);
    }
}

BOOL WINAPI terminalControlHandler(const DWORD eventType) {
    switch (eventType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (WindowsState* state = activeWindowsState.load(); state != nullptr) {
                emergencyRestoreWindows(*state);
            }
            return FALSE;

        default:
            return FALSE;
    }
}

#endif

} // namespace

namespace tt {

class TerminalSession::Implementation {
public:
    TerminalOptions options;
    bool active = false;
    bool alternateScreenActive = false;

#ifdef _WIN32
    WindowsState windows;
#endif

    explicit Implementation(TerminalOptions options)
        : options(std::move(options)) {}

    void configurePlatform() {
#ifdef _WIN32
        windows.outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

        if (windows.outputHandle == INVALID_HANDLE_VALUE || windows.outputHandle == nullptr) {
            throwWindowsError(
                TerminalErrorCode::InvalidOutputHandle,
                "terminalTool could not obtain the standard output handle."
            );
        }

        windows.inputHandle = GetStdHandle(STD_INPUT_HANDLE);

        if (windows.inputHandle == INVALID_HANDLE_VALUE || windows.inputHandle == nullptr) {
            throwWindowsError(
                TerminalErrorCode::InvalidInputHandle,
                "terminalTool could not obtain the standard input handle."
            );
        }

        if (!GetConsoleMode(windows.outputHandle, &windows.originalOutputMode)) {
            throwWindowsError(
                TerminalErrorCode::OutputIsNotTerminal,
                "terminalTool standard output is not an interactive Windows console."
            );
        }
        windows.outputModeSaved = true;

        if (!GetConsoleMode(windows.inputHandle, &windows.originalInputMode)) {
            throwWindowsError(
                TerminalErrorCode::InputIsNotTerminal,
                "terminalTool standard input is not an interactive Windows console."
            );
        }
        windows.inputModeSaved = true;

        windows.originalOutputCodePage = GetConsoleOutputCP();
        if (windows.originalOutputCodePage == 0) {
            throwWindowsError(
                TerminalErrorCode::SetOutputCodePageFailed,
                "terminalTool could not query the Windows console output code page."
            );
        }

        windows.originalInputCodePage = GetConsoleCP();
        if (windows.originalInputCodePage == 0) {
            throwWindowsError(
                TerminalErrorCode::SetInputCodePageFailed,
                "terminalTool could not query the Windows console input code page."
            );
        }

        if (!options.title.empty()) {
            std::vector<wchar_t> titleBuffer(32768, L'\0');
            SetLastError(ERROR_SUCCESS);
            const DWORD titleLength = GetConsoleTitleW(
                titleBuffer.data(),
                static_cast<DWORD>(titleBuffer.size())
            );
            const DWORD titleError = GetLastError();

            if (titleLength == 0 && titleError != ERROR_SUCCESS) {
                throw TerminalError(
                    TerminalErrorCode::QueryTitleFailed,
                    "terminalTool could not read the original Windows console title.",
                    static_cast<std::uint32_t>(titleError)
                );
            }

            windows.originalTitle.assign(titleBuffer.data(), titleLength);
            windows.titleSaved = true;
        }

        if (!SetConsoleOutputCP(CP_UTF8)) {
            throwWindowsError(
                TerminalErrorCode::SetOutputCodePageFailed,
                "terminalTool could not select UTF-8 console output."
            );
        }
        windows.outputCodePageChanged = true;

        if (!SetConsoleCP(CP_UTF8)) {
            throwWindowsError(
                TerminalErrorCode::SetInputCodePageFailed,
                "terminalTool could not select UTF-8 console input."
            );
        }
        windows.inputCodePageChanged = true;

        DWORD outputMode = windows.originalOutputMode;
        outputMode |= ENABLE_PROCESSED_OUTPUT;
        outputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

        if (!SetConsoleMode(windows.outputHandle, outputMode)) {
            throwWindowsError(
                TerminalErrorCode::ConfigureOutputModeFailed,
                "terminalTool could not enable Windows virtual-terminal output."
            );
        }

        DWORD inputMode = windows.originalInputMode;
        inputMode |= ENABLE_EXTENDED_FLAGS;
        inputMode |= ENABLE_WINDOW_INPUT;
        inputMode |= ENABLE_PROCESSED_INPUT;
        inputMode &= ~ENABLE_QUICK_EDIT_MODE;
        inputMode &= ~ENABLE_LINE_INPUT;
        inputMode &= ~ENABLE_ECHO_INPUT;
        inputMode &= ~ENABLE_MOUSE_INPUT;

        if (!SetConsoleMode(windows.inputHandle, inputMode)) {
            throwWindowsError(
                TerminalErrorCode::ConfigureInputModeFailed,
                "terminalTool could not configure Windows console input."
            );
        }

        if (!SetConsoleCtrlHandler(&terminalControlHandler, TRUE)) {
            throwWindowsError(
                TerminalErrorCode::InstallControlHandlerFailed,
                "terminalTool could not install its Windows console control handler."
            );
        }
        windows.controlHandlerInstalled = true;
        activeWindowsState.store(&windows);

        if (!options.title.empty()) {
            const std::wstring wideTitle = utf8ToWide(options.title);

            if (!SetConsoleTitleW(wideTitle.c_str())) {
                throwWindowsError(
                    TerminalErrorCode::SetTitleFailed,
                    "terminalTool could not set the Windows console title."
                );
            }
            windows.titleChanged = true;
        }
#else
        if (isatty(STDOUT_FILENO) == 0) {
            throw TerminalError(
                TerminalErrorCode::OutputIsNotTerminal,
                "terminalTool standard output is not an interactive terminal.",
                static_cast<std::uint32_t>(errno)
            );
        }

        if (isatty(STDIN_FILENO) == 0) {
            throw TerminalError(
                TerminalErrorCode::InputIsNotTerminal,
                "terminalTool standard input is not an interactive terminal.",
                static_cast<std::uint32_t>(errno)
            );
        }
#endif
    }

    void restorePlatform() noexcept {
#ifdef _WIN32
        removeControlHandler(windows);
        restoreWindowsState(windows);
#endif
    }

    void setAlternateScreenActive(const bool value) noexcept {
        alternateScreenActive = value;
#ifdef _WIN32
        windows.alternateScreenActive = value;
#endif
    }
};

TerminalSession::TerminalSession(const std::string& title)
    : TerminalSession(TerminalOptions { title, true }) {}

TerminalSession::TerminalSession(const TerminalOptions& options) {
    TerminalSession* expectedSession = nullptr;

    if (!activeSession.compare_exchange_strong(expectedSession, this)) {
        throw TerminalError(
            TerminalErrorCode::SessionAlreadyActive,
            "terminalTool permits only one TerminalSession at a time."
        );
    }

    try {
        implementation = std::make_unique<Implementation>(options);
        implementation->configurePlatform();

        const Console::Size size = Console::terminalSize();
        Console::initializeFrameBuffer(size.width, size.height);
        Input::initialize();

        std::string initializationSequence;

        if (options.alternateScreen) {
            implementation->setAlternateScreenActive(true);
            initializationSequence += "\033[?1049h";
        }

        initializationSequence += "\033[2J\033[H\033[?25l\033[?7l";
        Console::writeOutput(initializationSequence);
        implementation->active = true;
    } catch (...) {
        if (implementation) {
            std::string recoverySequence = "\033[0m\033[?7h\033[?25h";

            if (implementation->alternateScreenActive) {
                recoverySequence += "\033[?1049l";
            }

            writeNoThrow(recoverySequence);
            Console::shutdownFrameBuffer();
            Input::reset();
            implementation->restorePlatform();
            implementation->active = false;
        }

        TerminalSession* thisSession = this;
        activeSession.compare_exchange_strong(thisSession, nullptr);
        throw;
    }
}

TerminalSession::~TerminalSession() {
    if (!implementation || !implementation->active) {
        return;
    }

    std::string restoreSequence = "\033[0m\033[?7h\033[?25h";

    if (implementation->alternateScreenActive) {
        restoreSequence += "\033[?1049l";
    } else {
        restoreSequence +=
            "\033[" + std::to_string(Console::getFrameHeight() + 1) + ";1H\n";
    }

    writeNoThrow(restoreSequence);
    Console::shutdownFrameBuffer();
    Input::reset();
    implementation->restorePlatform();
    implementation->active = false;

    TerminalSession* thisSession = this;
    activeSession.compare_exchange_strong(thisSession, nullptr);
}

bool TerminalSession::update() {
    if (!implementation || !implementation->active) {
        return false;
    }

    return Console::resizeToTerminal();
}

bool TerminalSession::isActive() const noexcept {
    return implementation && implementation->active;
}

bool TerminalSession::hasActiveSession() noexcept {
    return activeSession.load() != nullptr;
}

} // namespace tt
