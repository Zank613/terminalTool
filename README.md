# terminalTool 0.3.0

`terminalTool` is a compact C++17 static library for terminal games and
interactive terminal tools. It provides a cell framebuffer, true-colour ANSI
rendering, cross-platform keyboard input, raw input events, focus reporting,
clipping regions, terminal restoration, and delta time. Its public API lives in
the `tt` namespace.

```cpp
#include <terminalTool/terminalTool.h>
```

## 0.3.0 highlights

- FIFO `tt::InputEvent` queue alongside the existing state API
- Rich key event metadata and complete modifier state
- Layout-neutral OEM key names with backward-compatible aliases
- Focus events and focus transition queries
- Expanded `tt::TerminalOptions`
- Windows, Linux, and macOS keyboard backends
- POSIX signal restoration
- Internal platform abstraction
- Incremental UTF-8/CSI/SS3 escape-sequence parser
- Nested clipping regions and RAII `tt::Console::ScopedClip`
- Expanded tests, CI, Doxygen comments, and usage guides

## Minimal loop

```cpp
#include <terminalTool/terminalTool.h>

int main() {
    tt::TerminalOptions options;
    options.title = "My Game";
    options.enableFocusEvents = true;

    tt::TerminalSession terminal(options);
    tt::DeltaTime deltaTime;
    bool running = true;

    while (running) {
        const double seconds = deltaTime.update();
        (void) terminal.update();
        tt::Input::update();

        if (tt::Input::isPressed(tt::Key::Escape)) {
            running = false;
        }

        tt::Console::beginFrame();
        tt::Console::drawText(2, 2, "Frame seconds: " + std::to_string(seconds));
        tt::Console::endFrame();
    }
}
```

## Raw events

Unconsumed events persist in FIFO order across frames:

```cpp
while (const std::optional<tt::InputEvent> event = tt::Input::pollEvent()) {
    if (event->type == tt::InputEventType::KeyPressed) {
        const tt::Key key = event->key.key;
        const bool repeated = event->key.repeated;
        const std::uint16_t scanCode = event->key.scanCode;
        const tt::ModifierState modifiers = event->key.modifiers;
    }
}
```

The convenient state API remains available:

```cpp
tt::Input::isPressed(tt::Key::Space);
tt::Input::isHeld(tt::Key::W);
tt::Input::isReleased(tt::Key::F1);
tt::Input::textInput();
tt::Input::isFocused();
tt::Input::focusGained();
tt::Input::focusLost();
```

POSIX terminals normally do not transmit physical key-release events.
terminalTool therefore represents each decoded POSIX key sequence as a
press/release pulse. `isPressed()` and native repeat sequences work normally;
continuous `isHeld()` state is naturally strongest on Windows.

## Physical keys and text

OEM names are layout-neutral:

```cpp
tt::Key::Oem1
tt::Key::Oem2
tt::Key::Oem102
```

Compatibility aliases such as `tt::Key::Semicolon` remain. Use `Key` for
bindings and `textInput()`/`TextEntered` for the actual character produced by
the user's keyboard layout.

## Clipping

```cpp
const tt::Console::Rect panelContents { 3, 4, 30, 10 };

{
    tt::Console::ScopedClip clip(panelContents);
    drawLargeMapOrList();
} // Previous clip is restored here.
```

Manual nesting is also available through `pushClip()`, `popClip()`,
`clearClips()`, and `currentClip()`.

## Terminal options

```cpp
tt::TerminalOptions options;
options.title = "My Game";
options.alternateScreen = true;
options.hideCursor = true;
options.disableLineWrapping = true;
options.enableFocusEvents = true;
options.clearOnStart = true;
options.clearOnExit = false;
options.installSignalHandlers = true;
options.restoreTitle = true;
```

## Platform behavior

- Windows uses `ReadConsoleInputW` and provides real key-down/key-up events,
  repeat counts, scan codes, left/right modifiers, lock state, and focus events.
- Linux and macOS use `termios`, non-blocking reads, and the shared escape parser.
- POSIX emergency restoration covers SIGINT, SIGTERM, SIGHUP, and SIGQUIT.
- SIGWINCH is observed while `TerminalSession::update()` remains the explicit
  resize point.

See `docs/input.md`, `docs/platforms.md`, `docs/clipping.md`, and
`docs/migration-0.2-to-0.3.md` for detailed contracts and examples.

## CMake

```cmake
add_subdirectory(external/terminalTool)
target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

Or install and consume it:

```cmake
find_package(terminalTool 0.3.0 CONFIG REQUIRED)
target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

The library remains static regardless of `BUILD_SHARED_LIBS`.

## Tests

```sh
cmake -S . -B build -DTERMINALTOOL_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

CI covers MSVC, MinGW, Ubuntu GCC, Ubuntu Clang, and macOS AppleClang.

## Doxygen

```sh
cmake -S . -B build
cmake --build build --target docs
```

Generated HTML starts at `build/documentation/html/index.html`.

## Licence and ownership

terminalTool is distributed under the Mozilla Public License 2.0. See
`LICENSE` and `NOTICE.md`.

Copyright (c) 2026 Ataerk YILDIRIM.
