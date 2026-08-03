/**
 * @file WindowsTerminal.cpp
 * @brief Implements the Windows console backend.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "platform/PlatformTerminal.h"

#include "terminalTool/TerminalError.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace {

struct WindowsState {
    tt::TerminalOptions options;
    HANDLE outputHandle = INVALID_HANDLE_VALUE;
    HANDLE inputHandle = INVALID_HANDLE_VALUE;
    DWORD originalOutputMode = 0;
    DWORD originalInputMode = 0;
    UINT originalOutputCodePage = 0;
    UINT originalInputCodePage = 0;
    std::wstring originalTitle;
    char16_t pendingHighSurrogate = 0;
    bool outputModeSaved = false;
    bool inputModeSaved = false;
    bool outputCodePageChanged = false;
    bool inputCodePageChanged = false;
    bool titleSaved = false;
    bool titleChanged = false;
    bool handlerInstalled = false;
    bool initialized = false;
};

WindowsState state;
std::atomic<WindowsState*> activeState { nullptr };

[[noreturn]] void throwWindows(const tt::TerminalErrorCode code, const char* message) {
    throw tt::TerminalError(code, message, static_cast<std::uint32_t>(GetLastError()));
}

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0
    );
    if (length <= 0) {
        throwWindows(
            tt::TerminalErrorCode::SetTitleFailed,
            "terminalTool could not decode the UTF-8 Windows console title."
        );
    }

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        length
    ) <= 0) {
        throwWindows(
            tt::TerminalErrorCode::SetTitleFailed,
            "terminalTool could not decode the UTF-8 Windows console title."
        );
    }
    return result;
}

std::string startupSequence(const tt::TerminalOptions& options) {
    std::string sequence;
    if (options.alternateScreen) sequence += "\033[?1049h";
    if (options.clearOnStart) sequence += "\033[2J\033[H";
    if (options.hideCursor) sequence += "\033[?25l";
    if (options.disableLineWrapping) sequence += "\033[?7l";
    return sequence;
}

std::string shutdownSequence(const tt::TerminalOptions& options) {
    std::string sequence = "\033[0m";
    if (options.disableLineWrapping) sequence += "\033[?7h";
    if (options.hideCursor) sequence += "\033[?25h";
    if (options.clearOnExit) sequence += "\033[2J\033[H";
    if (options.alternateScreen) sequence += "\033[?1049l";
    return sequence;
}

void writeBestEffort(const std::string& sequence) noexcept {
    if (state.outputHandle == INVALID_HANDLE_VALUE || state.outputHandle == nullptr) {
        return;
    }

    std::size_t offset = 0;
    while (offset < sequence.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
            sequence.size() - offset,
            static_cast<std::size_t>(0x7FFFFFFF)
        ));
        DWORD written = 0;
        if (!WriteFile(state.outputHandle, sequence.data() + offset, chunk, &written, nullptr) || written == 0) {
            return;
        }
        offset += written;
    }
}

void restoreState() noexcept {
    if (state.initialized) {
        writeBestEffort(shutdownSequence(state.options));
    } else {
        writeBestEffort("\033[0m\033[?7h\033[?25h\033[?1049l");
    }

    if (state.titleChanged && state.titleSaved && state.options.restoreTitle) {
        (void) SetConsoleTitleW(state.originalTitle.c_str());
    }
    if (state.outputModeSaved) {
        (void) SetConsoleMode(state.outputHandle, state.originalOutputMode);
    }
    if (state.inputModeSaved) {
        (void) SetConsoleMode(state.inputHandle, state.originalInputMode);
    }
    if (state.outputCodePageChanged && state.originalOutputCodePage != 0) {
        (void) SetConsoleOutputCP(state.originalOutputCodePage);
    }
    if (state.inputCodePageChanged && state.originalInputCodePage != 0) {
        (void) SetConsoleCP(state.originalInputCodePage);
    }
}

BOOL WINAPI controlHandler(const DWORD eventType) {
    switch (eventType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (activeState.load() != nullptr) {
                restoreState();
            }
            return FALSE;
        default:
            return FALSE;
    }
}

void removeHandler() noexcept {
    if (!state.handlerInstalled) {
        return;
    }
    (void) SetConsoleCtrlHandler(controlHandler, FALSE);
    state.handlerInstalled = false;
    activeState.store(nullptr);
}

tt::ModifierState modifiersFromControlState(const DWORD controlState) {
    tt::ModifierState modifiers;
    modifiers.leftShift = (GetKeyState(VK_LSHIFT) & 0x8000) != 0;
    modifiers.rightShift = (GetKeyState(VK_RSHIFT) & 0x8000) != 0;
    modifiers.leftControl = (controlState & LEFT_CTRL_PRESSED) != 0;
    modifiers.rightControl = (controlState & RIGHT_CTRL_PRESSED) != 0;
    modifiers.leftAlt = (controlState & LEFT_ALT_PRESSED) != 0;
    modifiers.rightAlt = (controlState & RIGHT_ALT_PRESSED) != 0;
    modifiers.leftSuper = (GetKeyState(VK_LWIN) & 0x8000) != 0;
    modifiers.rightSuper = (GetKeyState(VK_RWIN) & 0x8000) != 0;
    modifiers.capsLock = (controlState & CAPSLOCK_ON) != 0;
    modifiers.numLock = (controlState & NUMLOCK_ON) != 0;
    modifiers.scrollLock = (controlState & SCROLLLOCK_ON) != 0;
    modifiers.enhanced = (controlState & ENHANCED_KEY) != 0;

    if ((controlState & SHIFT_PRESSED) != 0 && !modifiers.leftShift && !modifiers.rightShift) {
        modifiers.leftShift = true;
    }
    return modifiers;
}

tt::Key keyFromVirtualCode(
    std::uint16_t virtualCode,
    const std::uint16_t scanCode,
    const std::uint32_t controlState
) {
    using tt::Key;

    if (virtualCode == VK_SHIFT) {
        virtualCode = static_cast<std::uint16_t>(MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX));
    } else if (virtualCode == VK_CONTROL) {
        virtualCode = (controlState & ENHANCED_KEY) != 0 ? VK_RCONTROL : VK_LCONTROL;
    } else if (virtualCode == VK_MENU) {
        virtualCode = (controlState & ENHANCED_KEY) != 0 ? VK_RMENU : VK_LMENU;
    } else if (virtualCode == VK_RETURN && (controlState & ENHANCED_KEY) != 0) {
        return Key::NumpadEnter;
    }

    if (virtualCode >= '0' && virtualCode <= '9') {
        return static_cast<Key>(static_cast<std::size_t>(Key::Zero) + (virtualCode - '0'));
    }
    if (virtualCode >= 'A' && virtualCode <= 'Z') {
        return static_cast<Key>(static_cast<std::size_t>(Key::A) + (virtualCode - 'A'));
    }
    if (virtualCode >= VK_F1 && virtualCode <= VK_F24) {
        return static_cast<Key>(static_cast<std::size_t>(Key::F1) + (virtualCode - VK_F1));
    }
    if (virtualCode >= VK_NUMPAD0 && virtualCode <= VK_NUMPAD9) {
        return static_cast<Key>(static_cast<std::size_t>(Key::Numpad0) + (virtualCode - VK_NUMPAD0));
    }

    switch (virtualCode) {
        case VK_CANCEL: return Key::Cancel;
        case VK_ESCAPE: return Key::Escape;
        case VK_BACK: return Key::Backspace;
        case VK_TAB: return Key::Tab;
        case VK_CLEAR: return Key::Clear;
        case VK_RETURN: return Key::Enter;
        case VK_PAUSE: return Key::Pause;
        case VK_CAPITAL: return Key::CapsLock;
        case VK_KANA: return Key::KanaHangul;
        case VK_JUNJA: return Key::Junja;
        case VK_FINAL: return Key::Final;
        case VK_HANJA: return Key::HanjaKanji;
#ifdef VK_IME_ON
        case VK_IME_ON: return Key::ImeOn;
#endif
#ifdef VK_IME_OFF
        case VK_IME_OFF: return Key::ImeOff;
#endif
        case VK_CONVERT: return Key::Convert;
        case VK_NONCONVERT: return Key::NonConvert;
        case VK_ACCEPT: return Key::Accept;
        case VK_MODECHANGE: return Key::ModeChange;
        case VK_SPACE: return Key::Space;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_END: return Key::End;
        case VK_HOME: return Key::Home;
        case VK_LEFT: return Key::Left;
        case VK_UP: return Key::Up;
        case VK_RIGHT: return Key::Right;
        case VK_DOWN: return Key::Down;
        case VK_SELECT: return Key::Select;
        case VK_PRINT: return Key::Print;
        case VK_EXECUTE: return Key::Execute;
        case VK_SNAPSHOT: return Key::PrintScreen;
        case VK_INSERT: return Key::Insert;
        case VK_DELETE: return Key::Delete;
        case VK_HELP: return Key::Help;
        case VK_LWIN: return Key::LeftSuper;
        case VK_RWIN: return Key::RightSuper;
        case VK_APPS: return Key::Menu;
        case VK_SLEEP: return Key::Sleep;
        case VK_MULTIPLY: return Key::NumpadMultiply;
        case VK_ADD: return Key::NumpadAdd;
        case VK_SEPARATOR: return Key::NumpadSeparator;
        case VK_SUBTRACT: return Key::NumpadSubtract;
        case VK_DECIMAL: return Key::NumpadDecimal;
        case VK_DIVIDE: return Key::NumpadDivide;
#ifdef VK_OEM_NEC_EQUAL
        case VK_OEM_NEC_EQUAL: return Key::NumpadEqual;
#endif
        case VK_NUMLOCK: return Key::NumLock;
        case VK_SCROLL: return Key::ScrollLock;
        case VK_LSHIFT: return Key::LeftShift;
        case VK_RSHIFT: return Key::RightShift;
        case VK_LCONTROL: return Key::LeftControl;
        case VK_RCONTROL: return Key::RightControl;
        case VK_LMENU: return Key::LeftAlt;
        case VK_RMENU: return Key::RightAlt;
        case VK_OEM_1: return Key::Oem1;
        case VK_OEM_PLUS: return Key::OemPlus;
        case VK_OEM_COMMA: return Key::OemComma;
        case VK_OEM_MINUS: return Key::OemMinus;
        case VK_OEM_PERIOD: return Key::OemPeriod;
        case VK_OEM_2: return Key::Oem2;
        case VK_OEM_3: return Key::Oem3;
        case VK_OEM_4: return Key::Oem4;
        case VK_OEM_5: return Key::Oem5;
        case VK_OEM_6: return Key::Oem6;
        case VK_OEM_7: return Key::Oem7;
        case VK_OEM_8: return Key::Oem8;
        case VK_OEM_102: return Key::Oem102;
        case VK_OEM_CLEAR: return Key::OemClear;
        case VK_BROWSER_BACK: return Key::BrowserBack;
        case VK_BROWSER_FORWARD: return Key::BrowserForward;
        case VK_BROWSER_REFRESH: return Key::BrowserRefresh;
        case VK_BROWSER_STOP: return Key::BrowserStop;
        case VK_BROWSER_SEARCH: return Key::BrowserSearch;
        case VK_BROWSER_FAVORITES: return Key::BrowserFavourites;
        case VK_BROWSER_HOME: return Key::BrowserHome;
        case VK_VOLUME_MUTE: return Key::VolumeMute;
        case VK_VOLUME_DOWN: return Key::VolumeDown;
        case VK_VOLUME_UP: return Key::VolumeUp;
        case VK_MEDIA_NEXT_TRACK: return Key::MediaNextTrack;
        case VK_MEDIA_PREV_TRACK: return Key::MediaPreviousTrack;
        case VK_MEDIA_STOP: return Key::MediaStop;
        case VK_MEDIA_PLAY_PAUSE: return Key::MediaPlayPause;
        case VK_LAUNCH_MAIL: return Key::LaunchMail;
        case VK_LAUNCH_MEDIA_SELECT: return Key::LaunchMediaSelect;
        case VK_LAUNCH_APP1: return Key::LaunchApp1;
        case VK_LAUNCH_APP2: return Key::LaunchApp2;
        case VK_PROCESSKEY: return Key::Process;
        case VK_PACKET: return Key::Packet;
        case VK_ATTN: return Key::Attn;
        case VK_CRSEL: return Key::CrSel;
        case VK_EXSEL: return Key::ExSel;
        case VK_EREOF: return Key::EraseEof;
        case VK_PLAY: return Key::Play;
        case VK_ZOOM: return Key::Zoom;
        case VK_NONAME: return Key::NoName;
        case VK_PA1: return Key::Pa1;
        default: return Key::Unknown;
    }
}

void appendTextCodeUnit(
    const char16_t codeUnit,
    const std::uint16_t repeatCount,
    const tt::ModifierState& modifiers,
    std::vector<tt::detail::NativeInputEvent>& events
) {
    for (std::uint16_t repeat = 0; repeat < repeatCount; repeat++) {
        if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF) {
            state.pendingHighSurrogate = codeUnit;
            continue;
        }

        char32_t character = codeUnit;
        if (codeUnit >= 0xDC00 && codeUnit <= 0xDFFF && state.pendingHighSurrogate != 0) {
            character = 0x10000 +
                ((static_cast<char32_t>(state.pendingHighSurrogate) - 0xD800) << 10) +
                (static_cast<char32_t>(codeUnit) - 0xDC00);
            state.pendingHighSurrogate = 0;
        } else if (codeUnit >= 0xDC00 && codeUnit <= 0xDFFF) {
            character = 0xFFFD;
        } else if (state.pendingHighSurrogate != 0) {
            tt::detail::NativeInputEvent replacement;
            replacement.type = tt::detail::NativeInputEventType::Text;
            replacement.character = 0xFFFD;
            replacement.modifiers = modifiers;
            events.push_back(replacement);
            state.pendingHighSurrogate = 0;
        }

        tt::detail::NativeInputEvent text;
        text.type = tt::detail::NativeInputEventType::Text;
        text.character = character;
        text.nativeKeyCode = static_cast<std::uint32_t>(character);
        text.modifiers = modifiers;
        events.push_back(text);
    }
}

} // namespace

namespace tt::detail {

void platformInitialize(const TerminalOptions& options) {
    state = WindowsState {};
    state.options = options;
    state.outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (state.outputHandle == INVALID_HANDLE_VALUE || state.outputHandle == nullptr) {
        throwWindows(TerminalErrorCode::InvalidOutputHandle, "terminalTool could not obtain standard output.");
    }
    state.inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    if (state.inputHandle == INVALID_HANDLE_VALUE || state.inputHandle == nullptr) {
        throwWindows(TerminalErrorCode::InvalidInputHandle, "terminalTool could not obtain standard input.");
    }
    if (!GetConsoleMode(state.outputHandle, &state.originalOutputMode)) {
        throwWindows(TerminalErrorCode::OutputIsNotTerminal, "terminalTool output is not an interactive Windows console.");
    }
    state.outputModeSaved = true;
    if (!GetConsoleMode(state.inputHandle, &state.originalInputMode)) {
        throwWindows(TerminalErrorCode::InputIsNotTerminal, "terminalTool input is not an interactive Windows console.");
    }
    state.inputModeSaved = true;

    state.originalOutputCodePage = GetConsoleOutputCP();
    state.originalInputCodePage = GetConsoleCP();
    if (state.originalOutputCodePage == 0) {
        throwWindows(TerminalErrorCode::SetOutputCodePageFailed, "terminalTool could not query the output code page.");
    }
    if (state.originalInputCodePage == 0) {
        throwWindows(TerminalErrorCode::SetInputCodePageFailed, "terminalTool could not query the input code page.");
    }

    if (!options.title.empty() && options.restoreTitle) {
        std::vector<wchar_t> title(32768, L'\0');
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetConsoleTitleW(title.data(), static_cast<DWORD>(title.size()));
        const DWORD error = GetLastError();
        if (length == 0 && error != ERROR_SUCCESS) {
            throw TerminalError(
                TerminalErrorCode::QueryTitleFailed,
                "terminalTool could not read the original Windows console title.",
                static_cast<std::uint32_t>(error)
            );
        }
        state.originalTitle.assign(title.data(), length);
        state.titleSaved = true;
    }

    if (!SetConsoleOutputCP(CP_UTF8)) {
        throwWindows(TerminalErrorCode::SetOutputCodePageFailed, "terminalTool could not enable UTF-8 output.");
    }
    state.outputCodePageChanged = true;
    if (!SetConsoleCP(CP_UTF8)) {
        restoreState();
        throwWindows(TerminalErrorCode::SetInputCodePageFailed, "terminalTool could not enable UTF-8 input.");
    }
    state.inputCodePageChanged = true;

    DWORD outputMode = state.originalOutputMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(state.outputHandle, outputMode)) {
        restoreState();
        throwWindows(TerminalErrorCode::ConfigureOutputModeFailed, "terminalTool could not enable virtual-terminal output.");
    }

    DWORD inputMode = state.originalInputMode;
    inputMode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_PROCESSED_INPUT;
    inputMode &= ~ENABLE_QUICK_EDIT_MODE;
    inputMode &= ~ENABLE_LINE_INPUT;
    inputMode &= ~ENABLE_ECHO_INPUT;
    inputMode &= ~ENABLE_MOUSE_INPUT;
    if (!SetConsoleMode(state.inputHandle, inputMode)) {
        restoreState();
        throwWindows(TerminalErrorCode::ConfigureInputModeFailed, "terminalTool could not configure Windows input.");
    }

    if (options.installSignalHandlers) {
        if (!SetConsoleCtrlHandler(controlHandler, TRUE)) {
            restoreState();
            throwWindows(TerminalErrorCode::InstallControlHandlerFailed, "terminalTool could not install its control handler.");
        }
        state.handlerInstalled = true;
        activeState.store(&state);
    }

    if (!options.title.empty()) {
        const std::wstring title = utf8ToWide(options.title);
        if (!SetConsoleTitleW(title.c_str())) {
            removeHandler();
            restoreState();
            throwWindows(TerminalErrorCode::SetTitleFailed, "terminalTool could not set the Windows console title.");
        }
        state.titleChanged = true;
    }

    try {
        platformWriteOutput(startupSequence(options));
    } catch (...) {
        removeHandler();
        restoreState();
        throw;
    }
    state.initialized = true;
}

void platformShutdown() noexcept {
    removeHandler();
    restoreState();
    state = WindowsState {};
}

TerminalDimensions platformTerminalSize() {
    CONSOLE_SCREEN_BUFFER_INFO information {};
    if (!GetConsoleScreenBufferInfo(state.outputHandle, &information)) {
        throwWindows(TerminalErrorCode::QueryTerminalSizeFailed, "terminalTool could not query the visible Windows console size.");
    }
    return TerminalDimensions {
        static_cast<int>(information.srWindow.Right - information.srWindow.Left + 1),
        static_cast<int>(information.srWindow.Bottom - information.srWindow.Top + 1)
    };
}

void platformWriteOutput(const std::string& output) {
    std::size_t offset = 0;
    while (offset < output.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
            output.size() - offset,
            static_cast<std::size_t>(0x7FFFFFFF)
        ));
        DWORD written = 0;
        if (!WriteFile(state.outputHandle, output.data() + offset, chunk, &written, nullptr)) {
            throwWindows(TerminalErrorCode::WriteOutputFailed, "terminalTool could not write Windows console output.");
        }
        if (written == 0) {
            throw TerminalError(TerminalErrorCode::WriteOutputFailed, "terminalTool wrote zero bytes to the Windows console.");
        }
        offset += written;
    }
}

void platformFlushInput() {
    if (!FlushConsoleInputBuffer(state.inputHandle)) {
        throwWindows(TerminalErrorCode::FlushInputFailed, "terminalTool could not flush pending Windows input.");
    }
    state.pendingHighSurrogate = 0;
}

void platformReadInput(std::vector<NativeInputEvent>& events) {
    DWORD available = 0;
    if (!GetNumberOfConsoleInputEvents(state.inputHandle, &available)) {
        throwWindows(TerminalErrorCode::QueryInputEventCountFailed, "terminalTool could not query pending Windows input.");
    }

    while (available > 0) {
        const DWORD requested = std::min<DWORD>(available, 128);
        std::vector<INPUT_RECORD> records(requested);
        DWORD read = 0;
        if (!ReadConsoleInputW(state.inputHandle, records.data(), requested, &read)) {
            throwWindows(TerminalErrorCode::ReadInputFailed, "terminalTool could not read Windows console input.");
        }

        for (DWORD index = 0; index < read; index++) {
            const INPUT_RECORD& record = records[index];

            if (record.EventType == KEY_EVENT) {
                const KEY_EVENT_RECORD& native = record.Event.KeyEvent;
                NativeInputEvent event;
                event.type = native.bKeyDown ? NativeInputEventType::KeyDown : NativeInputEventType::KeyUp;
                event.key = keyFromVirtualCode(native.wVirtualKeyCode, native.wVirtualScanCode, native.dwControlKeyState);
                event.repeated = native.bKeyDown && native.wRepeatCount > 1;
                event.repeatCount = std::max<std::uint16_t>(1, native.wRepeatCount);
                event.scanCode = native.wVirtualScanCode;
                event.nativeKeyCode = native.wVirtualKeyCode;
                event.modifiers = modifiersFromControlState(native.dwControlKeyState);
                events.push_back(event);

                if (native.bKeyDown && native.uChar.UnicodeChar != L'\0') {
                    appendTextCodeUnit(
                        static_cast<char16_t>(native.uChar.UnicodeChar),
                        event.repeatCount,
                        event.modifiers,
                        events
                    );
                }
            } else if (record.EventType == FOCUS_EVENT && state.options.enableFocusEvents) {
                NativeInputEvent event;
                event.type = record.Event.FocusEvent.bSetFocus
                    ? NativeInputEventType::FocusGained
                    : NativeInputEventType::FocusLost;
                events.push_back(event);
                if (!record.Event.FocusEvent.bSetFocus) {
                    state.pendingHighSurrogate = 0;
                }
            }
        }

        if (!GetNumberOfConsoleInputEvents(state.inputHandle, &available)) {
            throwWindows(TerminalErrorCode::QueryInputEventCountFailed, "terminalTool could not refresh pending Windows input count.");
        }
    }
}

} // namespace tt::detail
