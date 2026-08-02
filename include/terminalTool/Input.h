/**
 * @file Input.h
 * @brief Declares comprehensive keyboard and UTF-8 text input handling.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tt {

class TerminalSession;

/**
 * @brief Keyboard keys recognised by terminalTool's Windows console backend.
 *
 * The enumeration covers the standard alphanumeric keyboard, F1-F24,
 * navigation, numpad, left/right modifiers, punctuation, international and IME
 * keys, browser/media controls, application launch keys, and Windows legacy
 * keyboard virtual keys.
 */
enum class Key : std::size_t {
    Cancel, ///< Cancel or Ctrl+Break.
    Escape, ///< Escape.
    Backspace, ///< Backspace.
    Tab, ///< Tab.
    Clear, ///< Clear.
    Enter, ///< Main Enter or Return.
    Pause, ///< Pause.
    CapsLock, ///< Caps Lock.

    KanaHangul, ///< Kana or Hangul mode key.
    Junja, ///< Junja mode key.
    Final, ///< Final mode key.
    HanjaKanji, ///< Hanja or Kanji mode key.
    ImeOn, ///< IME On.
    ImeOff, ///< IME Off.
    Convert, ///< IME Convert.
    NonConvert, ///< IME NonConvert.
    Accept, ///< IME Accept.
    ModeChange, ///< IME mode change.

    Space, ///< Space bar.
    PageUp, ///< Page Up.
    PageDown, ///< Page Down.
    End, ///< End.
    Home, ///< Home.
    Left, ///< Left arrow.
    Up, ///< Up arrow.
    Right, ///< Right arrow.
    Down, ///< Down arrow.
    Select, ///< Select.
    Print, ///< Print.
    Execute, ///< Execute.
    PrintScreen, ///< Print Screen.
    Insert, ///< Insert.
    Delete, ///< Delete.
    Help, ///< Help.

    Zero, ///< Number-row 0.
    One, ///< Number-row 1.
    Two, ///< Number-row 2.
    Three, ///< Number-row 3.
    Four, ///< Number-row 4.
    Five, ///< Number-row 5.
    Six, ///< Number-row 6.
    Seven, ///< Number-row 7.
    Eight, ///< Number-row 8.
    Nine, ///< Number-row 9.

    A, ///< A.
    B, ///< B.
    C, ///< C.
    D, ///< D.
    E, ///< E.
    F, ///< F.
    G, ///< G.
    H, ///< H.
    I, ///< I.
    J, ///< J.
    K, ///< K.
    L, ///< L.
    M, ///< M.
    N, ///< N.
    O, ///< O.
    P, ///< P.
    Q, ///< Q.
    R, ///< R.
    S, ///< S.
    T, ///< T.
    U, ///< U.
    V, ///< V.
    W, ///< W.
    X, ///< X.
    Y, ///< Y.
    Z, ///< Z.

    LeftWindows, ///< Left Windows or Command key.
    RightWindows, ///< Right Windows or Command key.
    Menu, ///< Application menu key.
    Sleep, ///< System sleep key.

    Numpad0, ///< Numpad 0.
    Numpad1, ///< Numpad 1.
    Numpad2, ///< Numpad 2.
    Numpad3, ///< Numpad 3.
    Numpad4, ///< Numpad 4.
    Numpad5, ///< Numpad 5.
    Numpad6, ///< Numpad 6.
    Numpad7, ///< Numpad 7.
    Numpad8, ///< Numpad 8.
    Numpad9, ///< Numpad 9.
    NumpadMultiply, ///< Numpad multiply.
    NumpadAdd, ///< Numpad add.
    NumpadSeparator, ///< Numpad separator.
    NumpadSubtract, ///< Numpad subtract.
    NumpadDecimal, ///< Numpad decimal.
    NumpadDivide, ///< Numpad divide.
    NumpadEnter, ///< Numpad Enter.
    NumpadEqual, ///< Numpad equal on supported keyboards.

    F1, ///< Function key F1.
    F2, ///< Function key F2.
    F3, ///< Function key F3.
    F4, ///< Function key F4.
    F5, ///< Function key F5.
    F6, ///< Function key F6.
    F7, ///< Function key F7.
    F8, ///< Function key F8.
    F9, ///< Function key F9.
    F10, ///< Function key F10.
    F11, ///< Function key F11.
    F12, ///< Function key F12.
    F13, ///< Function key F13.
    F14, ///< Function key F14.
    F15, ///< Function key F15.
    F16, ///< Function key F16.
    F17, ///< Function key F17.
    F18, ///< Function key F18.
    F19, ///< Function key F19.
    F20, ///< Function key F20.
    F21, ///< Function key F21.
    F22, ///< Function key F22.
    F23, ///< Function key F23.
    F24, ///< Function key F24.

    NumLock, ///< Num Lock.
    ScrollLock, ///< Scroll Lock.
    LeftShift, ///< Left Shift.
    RightShift, ///< Right Shift.
    LeftControl, ///< Left Control.
    RightControl, ///< Right Control.
    LeftAlt, ///< Left Alt.
    RightAlt, ///< Right Alt or AltGr.

    Semicolon, ///< Semicolon or colon key.
    Equal, ///< Equal or plus key.
    Comma, ///< Comma or less-than key.
    Minus, ///< Minus or underscore key.
    Period, ///< Period or greater-than key.
    Slash, ///< Slash or question-mark key.
    Grave, ///< Grave accent or tilde key.
    LeftBracket, ///< Left bracket or brace key.
    Backslash, ///< Backslash or pipe key.
    RightBracket, ///< Right bracket or brace key.
    Apostrophe, ///< Apostrophe or quotation-mark key.
    NonUsBackslash, ///< Non-US backslash key.
    Oem8, ///< OEM-specific keyboard key.
    OemClear, ///< OEM Clear key.

    BrowserBack, ///< Browser Back.
    BrowserForward, ///< Browser Forward.
    BrowserRefresh, ///< Browser Refresh.
    BrowserStop, ///< Browser Stop.
    BrowserSearch, ///< Browser Search.
    BrowserFavourites, ///< Browser Favourites.
    BrowserHome, ///< Browser Home.

    VolumeMute, ///< Mute volume.
    VolumeDown, ///< Lower volume.
    VolumeUp, ///< Raise volume.
    MediaNextTrack, ///< Next media track.
    MediaPreviousTrack, ///< Previous media track.
    MediaStop, ///< Stop media playback.
    MediaPlayPause, ///< Play or pause media.
    LaunchMail, ///< Launch mail application.
    LaunchMediaSelect, ///< Launch media selector.
    LaunchApp1, ///< Launch application 1.
    LaunchApp2, ///< Launch application 2.

    Process, ///< IME Process key.
    Packet, ///< Unicode packet input key.
    Attn, ///< Attention key.
    CrSel, ///< Cursor Select key.
    ExSel, ///< Extend Selection key.
    EraseEof, ///< Erase End Of File key.
    Play, ///< Play key.
    Zoom, ///< Zoom key.
    NoName, ///< Reserved NoName key.
    Pa1, ///< PA1 key.

    Count ///< Sentinel used internally; not a usable key.
};

/**
 * @brief Static per-frame keyboard and UTF-8 text input API.
 *
 * On Windows, input is read from console events rather than global keyboard
 * polling. Call update() exactly once near the start of each frame.
 */
class Input {
private:
    friend class TerminalSession;
    static constexpr std::size_t KEY_COUNT = static_cast<std::size_t>(Key::Count);

    static std::array<bool, KEY_COUNT> current;
    static std::array<bool, KEY_COUNT> pressed;
    static std::array<bool, KEY_COUNT> released;
    static std::string text;
    static char16_t pendingHighSurrogate;
    static bool focused;

    [[nodiscard]] static std::size_t index(Key key) noexcept;
    [[nodiscard]] static Key keyFromVirtualCode(std::uint16_t virtualCode, std::uint16_t scanCode, std::uint32_t controlState);
    static void setKeyState(Key key, bool isDown);
    static void releaseAllKeys();
    static void appendTextCodeUnit(char16_t codeUnit, std::uint16_t repeatCount);
    static void appendUtf8(char32_t character);

    /** @brief Resets input state and clears pending Windows console events. */
    static void initialize();

    /** @brief Clears all held, pressed, released, focus, and text state. */
    static void reset() noexcept;

public:
    /**
     * @brief Reads available input events and refreshes per-frame states.
     * @throws TerminalError when the console input handle or event stream fails.
     */
    static void update();

    /**
     * @param key Key to test.
     * @return `true` while the key remains down. `Key::Count` returns `false`.
     */
    [[nodiscard]] static bool isHeld(Key key) noexcept;

    /**
     * @param key Key to test.
     * @return `true` only during the frame in which the key goes down.
     */
    [[nodiscard]] static bool isPressed(Key key) noexcept;

    /**
     * @param key Key to test.
     * @return `true` only during the frame in which the key goes up.
     */
    [[nodiscard]] static bool isReleased(Key key) noexcept;

    /** @return `true` when the console currently has input focus. */
    [[nodiscard]] static bool isFocused() noexcept;

    /**
     * @brief Returns printable UTF-8 text received by the latest update().
     * @return Reference valid until the next update() or reset().
     */
    [[nodiscard]] static const std::string& textInput() noexcept;
};

} // namespace tt
