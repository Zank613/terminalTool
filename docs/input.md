# Input

Call `tt::Input::update()` once per frame, after `TerminalSession::update()`.
The state API is per-frame; the raw event queue persists until consumed.

## State API

```cpp
tt::Input::isPressed(tt::Key::Space);
tt::Input::isHeld(tt::Key::W);
tt::Input::isReleased(tt::Key::Escape);
tt::Input::textInput();
```

Windows provides genuine key-up state. POSIX terminals normally provide only
byte sequences, so terminalTool emits press/release pulses there.

## Raw events

`InputEvent` preserves repeat count, scan code, native code, and modifier state
where the platform supplies them. The queue is FIFO and bounded by
`TerminalOptions::maximumQueuedInputEvents`.

When full, the oldest event is removed before the new event is added:

```cpp
if (tt::Input::eventOverflowed()) {
    log(tt::Input::droppedEventCount());
    tt::Input::clearEventOverflow();
}
```

A limit of zero disables event retention without disabling the state API.

## Keys versus text

Use layout-neutral `Oem1` through `Oem102` for bindings and `textInput()` or
`TextEntered` for the actual character produced by the active keyboard layout.
Compatibility aliases such as `Semicolon` remain.

Traditional POSIX byte streams cannot always identify a physical key. Shifted
ASCII digits are mapped to their conventional digit keys with Shift metadata.
Ctrl+Space, Ctrl+Backslash, Ctrl+RightBracket, Ctrl+^, and Ctrl+_ are decoded.
Ctrl+[ remains indistinguishable from Escape in traditional terminals.

## Parser recovery

Pending CSI/SS3 input is incremental and may span reads. Escape sequences are
bounded to 128 bytes. Malformed or overlong sequences recover as ordinary input
rather than growing the parser buffer indefinitely. Numeric CSI fields use
checked `std::from_chars` parsing.

Malformed UTF-8 produces U+FFFD with predictable byte advancement.
