# Migrating from 0.2.x to 0.3.0

Most 0.2.x source code remains valid.

## Existing input calls

These are unchanged:

```cpp
tt::Input::update();
tt::Input::isPressed(tt::Key::W);
tt::Input::isHeld(tt::Key::W);
tt::Input::isReleased(tt::Key::W);
tt::Input::textInput();
```

## OEM keys

Punctuation keys now have layout-neutral canonical names:

```cpp
tt::Key::Oem1;
tt::Key::OemPlus;
tt::Key::Oem102;
```

Old names such as `Semicolon`, `Equal`, and `NonUsBackslash` remain aliases, so
an immediate rename is optional.

## TerminalOptions

Aggregate initialization with exactly two values still follows `title` and
`alternateScreen`, but named assignment is recommended because more options now
exist:

```cpp
tt::TerminalOptions options;
options.title = "My Game";
options.alternateScreen = true;
```

## POSIX input

Linux and macOS input is now active. Because terminals do not report key-up,
POSIX keys are press/release pulses. Code based on `isPressed()` works directly.
Real-time movement may combine `isHeld()` and `isPressed()` for identical source
across Windows and POSIX.

## Raw events

New code may consume `tt::InputEvent` objects. This does not disable or drain
the per-frame state API.
