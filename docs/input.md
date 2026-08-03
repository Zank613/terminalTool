# Input model

terminalTool 0.3.0 provides two views of the same platform input stream:

1. Per-frame state through `tt::Input::isPressed`, `isHeld`, `isReleased`,
   `textInput`, and focus queries.
2. A persistent FIFO queue through `tt::Input::pollEvent`.

Call `tt::Input::update()` once near the beginning of each frame. It clears only
per-frame transition and text state. Raw events remain queued until consumed or
`clearEvents()` is called.

## Event payloads

`KeyPressed` and `KeyReleased` carry `tt::KeyEventData`:

- Portable `tt::Key`
- Repeat flag and native repeat count
- Native scan code when available
- Native virtual key or sequence code
- Complete `tt::ModifierState`

`TextEntered` carries one Unicode code point. It is intentionally separate from
the physical/logical key because keyboard layout, Shift, Caps Lock, AltGr, and
IME processing can change produced text.

## Modifier state

Windows reports left/right Control and Alt directly and terminalTool queries
left/right Shift and Super state. POSIX escape protocols normally report only
aggregate modifiers. terminalTool records aggregate POSIX state in the
corresponding left-side field.

Use the convenience functions:

```cpp
modifiers.shift();
modifiers.control();
modifiers.alt();
modifiers.super();
modifiers.altGr();
```

## POSIX release behavior

Traditional terminals send bytes and escape sequences, not physical keyboard
release records. Linux and macOS therefore emit a key press followed by a
synthetic release for each decoded key sequence. Repeated keys arrive as
repeated pulses from the terminal. Windows retains genuine held state.

## Escape ambiguity

A lone Escape byte can also begin a CSI, SS3, or Alt-modified sequence. The
shared parser waits briefly before accepting it as `tt::Key::Escape`, allowing
split non-blocking reads to complete first.
