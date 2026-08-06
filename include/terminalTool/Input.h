/**
 * @file Input.h
 * @brief Declares cross-platform keyboard state, text input, and raw events.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace tt {

class TerminalSession;
namespace detail {
class TestAccess;
struct NativeInputEvent;
}

/**
 * @brief Keyboard keys recognised by terminalTool.
 *
 * Named OEM entries are deliberately layout-neutral. Use textInput() or
 * InputEventType::TextEntered when the produced character matters; use Key for
 * game controls and physical/logical key bindings.
 */
enum class Key : std::size_t {
    Unknown,
    Cancel,
    Escape,
    Backspace,
    Tab,
    Clear,
    Enter,
    Pause,
    CapsLock,

    KanaHangul,
    Junja,
    Final,
    HanjaKanji,
    ImeOn,
    ImeOff,
    Convert,
    NonConvert,
    Accept,
    ModeChange,

    Space,
    PageUp,
    PageDown,
    End,
    Home,
    Left,
    Up,
    Right,
    Down,
    Select,
    Print,
    Execute,
    PrintScreen,
    Insert,
    Delete,
    Help,

    Zero,
    One,
    Two,
    Three,
    Four,
    Five,
    Six,
    Seven,
    Eight,
    Nine,

    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    LeftSuper,
    RightSuper,
    Menu,
    Sleep,

    Numpad0,
    Numpad1,
    Numpad2,
    Numpad3,
    Numpad4,
    Numpad5,
    Numpad6,
    Numpad7,
    Numpad8,
    Numpad9,
    NumpadMultiply,
    NumpadAdd,
    NumpadSeparator,
    NumpadSubtract,
    NumpadDecimal,
    NumpadDivide,
    NumpadEnter,
    NumpadEqual,

    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,

    NumLock,
    ScrollLock,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,

    Oem1,
    OemPlus,
    OemComma,
    OemMinus,
    OemPeriod,
    Oem2,
    Oem3,
    Oem4,
    Oem5,
    Oem6,
    Oem7,
    Oem8,
    Oem102,
    OemClear,

    BrowserBack,
    BrowserForward,
    BrowserRefresh,
    BrowserStop,
    BrowserSearch,
    BrowserFavourites,
    BrowserHome,

    VolumeMute,
    VolumeDown,
    VolumeUp,
    MediaNextTrack,
    MediaPreviousTrack,
    MediaStop,
    MediaPlayPause,
    LaunchMail,
    LaunchMediaSelect,
    LaunchApp1,
    LaunchApp2,

    Process,
    Packet,
    Attn,
    CrSel,
    ExSel,
    EraseEof,
    Play,
    Zoom,
    NoName,
    Pa1,

    Count,

    // Compatibility aliases retained from terminalTool 0.2.x.
    LeftWindows = LeftSuper,
    RightWindows = RightSuper,
    Semicolon = Oem1,
    Equal = OemPlus,
    Comma = OemComma,
    Minus = OemMinus,
    Period = OemPeriod,
    Slash = Oem2,
    Grave = Oem3,
    LeftBracket = Oem4,
    Backslash = Oem5,
    RightBracket = Oem6,
    Apostrophe = Oem7,
    NonUsBackslash = Oem102
};

/**
 * @brief Modifier and lock-key state attached to a key event.
 *
 * POSIX terminals normally report aggregate Shift, Control, Alt, and Super
 * information rather than left/right physical sides. In that case terminalTool
 * places the aggregate state in the corresponding left-side field and leaves
 * the right-side field false.
 */
struct ModifierState {
    bool leftShift = false;
    bool rightShift = false;
    bool leftControl = false;
    bool rightControl = false;
    bool leftAlt = false;
    bool rightAlt = false;
    bool leftSuper = false;
    bool rightSuper = false;
    bool capsLock = false;
    bool numLock = false;
    bool scrollLock = false;
    bool enhanced = false;

    /** @return Whether either Shift key is active. */
    [[nodiscard]] bool shift() const noexcept;
    /** @return Whether either Control key is active. */
    [[nodiscard]] bool control() const noexcept;
    /** @return Whether either Alt key is active. */
    [[nodiscard]] bool alt() const noexcept;
    /** @return Whether either Super/Windows/Command key is active. */
    [[nodiscard]] bool super() const noexcept;
    /** @return Whether Right Alt is active, commonly representing AltGr. */
    [[nodiscard]] bool altGr() const noexcept;
};

/** @brief Kinds of raw input events produced by Input::pollEvent(). */
enum class InputEventType {
    KeyPressed,   ///< A key-down event, including native repeat events.
    KeyReleased,  ///< A key-up event or a synthetic POSIX pulse release.
    TextEntered,  ///< One decoded Unicode code point was entered.
    FocusGained,  ///< The terminal reported input focus gained.
    FocusLost     ///< The terminal reported input focus lost.
};

/** @brief Complete native and portable metadata for a keyboard event. */
struct KeyEventData {
    Key key = Key::Unknown;              ///< Portable terminalTool key.
    bool repeated = false;               ///< Whether this is a repeat key-down.
    std::uint16_t repeatCount = 1;        ///< Native repeat count when available.
    std::uint16_t scanCode = 0;           ///< Native scan code, or zero on POSIX.
    std::uint32_t nativeKeyCode = 0;      ///< Native virtual key/code/sequence value.
    ModifierState modifiers {};           ///< Modifier state at event time.
};

/**
 * @brief One raw event retained in terminalTool's FIFO input queue.
 *
 * For KeyPressed and KeyReleased, inspect key. For TextEntered, inspect
 * character. Focus events carry no additional payload.
 */
struct InputEvent {
    InputEventType type = InputEventType::KeyPressed;
    KeyEventData key {};
    char32_t character = U'\0';
};

/**
 * @brief Static per-frame keyboard, UTF-8 text, focus, and raw-event API.
 *
 * Call update() once near the start of each frame. Per-frame state is replaced
 * on every update, while unconsumed raw events remain in the FIFO queue until
 * pollEvent() or clearEvents() removes them.
 */
class Input {
private:
    friend class TerminalSession;
    friend class detail::TestAccess;

    static constexpr std::size_t KEY_COUNT = static_cast<std::size_t>(Key::Count);

    static std::array<bool, KEY_COUNT> current;
    static std::array<bool, KEY_COUNT> pressed;
    static std::array<bool, KEY_COUNT> released;
    static std::string text;
    static std::deque<InputEvent> eventQueue;
    static std::size_t maximumEventQueueSize;
    static std::size_t droppedEvents;
    static bool queueOverflowed;
    static bool focused;
    static bool gainedFocus;
    static bool lostFocus;

    [[nodiscard]] static std::size_t index(Key key) noexcept;
    static void initialize(std::size_t maximumQueuedEvents);
    static void reset() noexcept;
    static void processNativeEvent(const detail::NativeInputEvent& event);
    static void setKeyState(KeyEventData key, bool isDown);
    static void releaseAllKeys(const ModifierState& modifiers = {});
    static void appendUtf8(char32_t character);
    static void enqueue(InputEvent event) noexcept;

public:
    /**
     * @brief Reads all currently available platform input and updates state.
     * @throws TerminalError when the platform input stream fails.
     */
    static void update();

    /** @return Whether key is currently held. */
    [[nodiscard]] static bool isHeld(Key key) noexcept;
    /** @return Whether key transitioned down during the latest update(). */
    [[nodiscard]] static bool isPressed(Key key) noexcept;
    /** @return Whether key transitioned up during the latest update(). */
    [[nodiscard]] static bool isReleased(Key key) noexcept;

    /** @return Whether the terminal currently reports input focus. */
    [[nodiscard]] static bool isFocused() noexcept;
    /** @return Whether focus was gained during the latest update(). */
    [[nodiscard]] static bool focusGained() noexcept;
    /** @return Whether focus was lost during the latest update(). */
    [[nodiscard]] static bool focusLost() noexcept;

    /**
     * @return UTF-8 text entered during the latest update().
     * @note This value is cleared at the start of the next update().
     */
    [[nodiscard]] static const std::string& textInput() noexcept;

    /** @return Number of unconsumed raw input events. */
    [[nodiscard]] static std::size_t eventCount() noexcept;
    /** @return Whether at least one raw event is waiting. */
    [[nodiscard]] static bool hasEvent() noexcept;

    /**
     * @brief Removes and returns the oldest raw input event.
     * @return Empty optional when the queue is empty.
     */
    [[nodiscard]] static std::optional<InputEvent> pollEvent();

    /** @brief Discards all queued raw events without changing key state. */
    static void clearEvents() noexcept;

    /** @return Number of events discarded because the queue was full. */
    [[nodiscard]] static std::size_t droppedEventCount() noexcept;

    /** @return Whether at least one event has overflowed since the flag was cleared. */
    [[nodiscard]] static bool eventOverflowed() noexcept;

    /** @brief Clears the overflow flag and discarded-event counter. */
    static void clearEventOverflow() noexcept;
};

} // namespace tt
