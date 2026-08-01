/**
 * @file TerminalSession.cpp
 * @brief Implements tt::TerminalSession setup, errors, and restoration.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/TerminalSession.h"

#include <cstdint>
#include <iostream>
#include <new>
#include <string>

#ifndef _WIN32
#include <cerrno>
#include <unistd.h>
#endif

#include "terminalTool/Colour.h"
#include "terminalTool/Console.h"
#include "terminalTool/Input.h"
#include "terminalTool/TerminalError.h"

namespace {

#ifdef _WIN32
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
        throwWindowsError(tt::TerminalErrorCode::SetTitleFailed, "terminalTool could not decode the UTF-8 terminal title.");
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
        throwWindowsError(tt::TerminalErrorCode::SetTitleFailed, "terminalTool could not decode the UTF-8 terminal title.");
    }

    return result;
}
#endif

} // namespace

namespace tt {

std::atomic<TerminalSession*> TerminalSession::activeSession { nullptr };

TerminalSession::TerminalSession(const std::string& title) {
    TerminalSession* expectedSession = nullptr;

    if (!activeSession.compare_exchange_strong(expectedSession, this)) {
        throw TerminalError(
            TerminalErrorCode::SessionAlreadyActive,
            "terminalTool permits only one TerminalSession at a time."
        );
    }

    try {
        configure(title);

        const Console::Size size = Console::terminalSize();

        try {
            Console::initializeFrameBuffer(size.width, size.height);
        } catch (const std::bad_alloc&) {
            throw TerminalError(
                TerminalErrorCode::FrameBufferInitializationFailed,
                "terminalTool could not allocate its terminal framebuffer."
            );
        }

        if (!Console::isActive()) {
            throw TerminalError(
                TerminalErrorCode::FrameBufferInitializationFailed,
                "terminalTool could not initialize its terminal framebuffer."
            );
        }

        Input::initialize();

        std::cout
            << "\033[2J"
            << "\033[H"
            << "\033[?25l"
            << "\033[?7l";
        std::cout.flush();

        if (!std::cout.good()) {
            throw TerminalError(
                TerminalErrorCode::ConfigureOutputModeFailed,
                "terminalTool could not write initialization sequences to the terminal."
            );
        }

        active = true;
    } catch (...) {
        Console::shutdownFrameBuffer();
        Input::reset();
        restoreSystemState();
        TerminalSession* thisSession = this;
        activeSession.compare_exchange_strong(thisSession, nullptr);
        throw;
    }
}

TerminalSession::~TerminalSession() {
    restore();
}

bool TerminalSession::update() {
    if (!active) {
        return false;
    }

    return Console::resizeToTerminal();
}

bool TerminalSession::isActive() const noexcept {
    return active;
}

bool TerminalSession::hasActiveSession() noexcept {
    return activeSession.load() != nullptr;
}

void TerminalSession::configure(const std::string& title) {
    std::ios::sync_with_stdio(false);

#ifdef _WIN32
    outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (outputHandle == INVALID_HANDLE_VALUE || outputHandle == nullptr) {
        throwWindowsError(TerminalErrorCode::InvalidOutputHandle, "terminalTool could not obtain the standard output handle.");
    }

    inputHandle = GetStdHandle(STD_INPUT_HANDLE);

    if (inputHandle == INVALID_HANDLE_VALUE || inputHandle == nullptr) {
        throwWindowsError(TerminalErrorCode::InvalidInputHandle, "terminalTool could not obtain the standard input handle.");
    }

    if (!GetConsoleMode(outputHandle, &originalOutputMode)) {
        throwWindowsError(TerminalErrorCode::OutputIsNotTerminal, "terminalTool standard output is not an interactive Windows console.");
    }

    outputModeSaved = true;

    if (!GetConsoleMode(inputHandle, &originalInputMode)) {
        throwWindowsError(TerminalErrorCode::InputIsNotTerminal, "terminalTool standard input is not an interactive Windows console.");
    }

    inputModeSaved = true;
    originalOutputCodePage = GetConsoleOutputCP();

    if (originalOutputCodePage == 0) {
        throwWindowsError(TerminalErrorCode::SetOutputCodePageFailed, "terminalTool could not query the Windows console output code page.");
    }

    originalInputCodePage = GetConsoleCP();

    if (originalInputCodePage == 0) {
        throwWindowsError(TerminalErrorCode::SetInputCodePageFailed, "terminalTool could not query the Windows console input code page.");
    }

    if (!SetConsoleOutputCP(CP_UTF8)) {
        throwWindowsError(TerminalErrorCode::SetOutputCodePageFailed, "terminalTool could not select UTF-8 console output.");
    }

    outputCodePageChanged = true;

    if (!SetConsoleCP(CP_UTF8)) {
        throwWindowsError(TerminalErrorCode::SetInputCodePageFailed, "terminalTool could not select UTF-8 console input.");
    }

    inputCodePageChanged = true;

    DWORD outputMode = originalOutputMode;
    outputMode |= ENABLE_PROCESSED_OUTPUT;
    outputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(outputHandle, outputMode)) {
        throwWindowsError(TerminalErrorCode::ConfigureOutputModeFailed, "terminalTool could not enable Windows virtual-terminal output.");
    }

    DWORD inputMode = originalInputMode;
    inputMode |= ENABLE_EXTENDED_FLAGS;
    inputMode |= ENABLE_WINDOW_INPUT;
    inputMode |= ENABLE_PROCESSED_INPUT;
    inputMode &= ~ENABLE_QUICK_EDIT_MODE;
    inputMode &= ~ENABLE_LINE_INPUT;
    inputMode &= ~ENABLE_ECHO_INPUT;
    inputMode &= ~ENABLE_MOUSE_INPUT;

    if (!SetConsoleMode(inputHandle, inputMode)) {
        throwWindowsError(TerminalErrorCode::ConfigureInputModeFailed, "terminalTool could not configure Windows console input.");
    }

    if (!SetConsoleCtrlHandler(&TerminalSession::controlHandler, TRUE)) {
        throwWindowsError(TerminalErrorCode::InstallControlHandlerFailed, "terminalTool could not install its Windows console control handler.");
    }

    controlHandlerInstalled = true;

    if (!title.empty()) {
        const std::wstring wideTitle = utf8ToWide(title);

        if (!SetConsoleTitleW(wideTitle.c_str())) {
            throwWindowsError(TerminalErrorCode::SetTitleFailed, "terminalTool could not set the Windows console title.");
        }
    }
#else
    (void) title;

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

void TerminalSession::restore() noexcept {
    if (!active) {
        return;
    }

    const int finalRow = Console::getFrameHeight();

    std::cout
        << Colour::RESET
        << "\033[?7h"
        << "\033[?25h"
        << "\033[" << finalRow + 1 << ";1H"
        << '\n';
    std::cout.flush();

    Console::shutdownFrameBuffer();
    Input::reset();
    restoreSystemState();

    active = false;

    TerminalSession* thisSession = this;
    activeSession.compare_exchange_strong(thisSession, nullptr);
}

void TerminalSession::restoreSystemState() noexcept {
#ifdef _WIN32
    if (controlHandlerInstalled) {
        SetConsoleCtrlHandler(&TerminalSession::controlHandler, FALSE);
        controlHandlerInstalled = false;
    }

    if (outputModeSaved && outputHandle != INVALID_HANDLE_VALUE && outputHandle != nullptr) {
        SetConsoleMode(outputHandle, originalOutputMode);
    }

    if (inputModeSaved && inputHandle != INVALID_HANDLE_VALUE && inputHandle != nullptr) {
        SetConsoleMode(inputHandle, originalInputMode);
    }

    if (outputCodePageChanged && originalOutputCodePage != 0) {
        SetConsoleOutputCP(originalOutputCodePage);
        outputCodePageChanged = false;
    }

    if (inputCodePageChanged && originalInputCodePage != 0) {
        SetConsoleCP(originalInputCodePage);
        inputCodePageChanged = false;
    }
#endif
}

#ifdef _WIN32
BOOL WINAPI TerminalSession::controlHandler(const DWORD eventType) {
    switch (eventType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (TerminalSession* session = activeSession.load(); session != nullptr) {
                session->emergencyRestore();
            }

            return FALSE;

        default:
            return FALSE;
    }
}

void TerminalSession::emergencyRestore() const noexcept {
    if (outputModeSaved && outputHandle != INVALID_HANDLE_VALUE && outputHandle != nullptr) {
        static constexpr char RESTORE_SEQUENCE[] = "\033[0m\033[?7h\033[?25h";
        DWORD bytesWritten = 0;

        WriteFile(
            outputHandle,
            RESTORE_SEQUENCE,
            static_cast<DWORD>(sizeof(RESTORE_SEQUENCE) - 1),
            &bytesWritten,
            nullptr
        );

        SetConsoleMode(outputHandle, originalOutputMode);
    }

    if (inputModeSaved && inputHandle != INVALID_HANDLE_VALUE && inputHandle != nullptr) {
        SetConsoleMode(inputHandle, originalInputMode);
    }

    if (outputCodePageChanged && originalOutputCodePage != 0) {
        SetConsoleOutputCP(originalOutputCodePage);
    }

    if (inputCodePageChanged && originalInputCodePage != 0) {
        SetConsoleCP(originalInputCodePage);
    }
}
#endif

} // namespace tt
