# terminalTool 0.3.1

`terminalTool` is a compact C++17 static library for terminal games and
interactive terminal tools. It provides a cell framebuffer, true-colour ANSI
rendering, cross-platform keyboard input, bounded raw input events, focus
reporting, clipping, terminal restoration, and delta time. Its public API lives
in the `tt` namespace.

```cpp
#include <terminalTool/terminalTool.h>
```

## 0.3.1 hardening highlights

- Version metadata comes from `project(VERSION ...)` only; CI contains no
  release-number literals.
- `tt::TerminalErrorCode::NoActiveSession` catches invalid runtime use.
- Raw events are bounded and report discarded events.
- Strict shared UTF-8 handling replaces malformed data predictably.
- Terminal control characters and POSIX title injection are sanitized.
- POSIX suspension restores the terminal and resume reapplies it.
- Previous POSIX signal handlers are preserved and chained.
- Windows modifier snapshots and repeated UTF-16 surrogate pairs are hardened.
- Rectangle operations avoid signed overflow and huge off-screen line loops.
- Subproject builds do not create tests, examples, or docs unless requested.

## Minimal loop

```cpp
#include <terminalTool/terminalTool.h>

int main() {
    tt::TerminalOptions options;
    options.title = "My Game";
    options.maximumQueuedInputEvents = 4096;

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

Call `TerminalSession::update()` before `Input::update()`. This lets POSIX
sessions reapply raw mode after `SIGCONT` before input is read.

## Bounded raw events

```cpp
while (const std::optional<tt::InputEvent> event = tt::Input::pollEvent()) {
    if (event->type == tt::InputEventType::KeyPressed) {
        const tt::Key key = event->key.key;
        const tt::ModifierState modifiers = event->key.modifiers;
    }
}

if (tt::Input::eventOverflowed()) {
    const std::size_t lost = tt::Input::droppedEventCount();
    tt::Input::clearEventOverflow();
}
```

Set `maximumQueuedInputEvents` to zero to disable raw-event retention while
keeping `isPressed()`, `isHeld()`, `isReleased()`, and `textInput()` active.

## Safe drawing and clipping

Framebuffer control scalars such as Escape, newline, NUL, DEL, and C1 controls
are replaced with U+FFFD instead of being emitted as terminal commands.

```cpp
const tt::Console::Rect contents { 3, 4, 30, 10 };
{
    tt::Console::ScopedClip clip(contents);
    drawLargeMapOrList();
}
```

Manual `pushClip()`/`popClip()` remains available. A mismatched `popClip()` is
asserted in debug builds and ignored safely in release builds.

## Error handling

```cpp
try {
    tt::TerminalSession terminal("My Game");
    // ...
} catch (const tt::TerminalError& error) {
    std::cerr << error.what() << " (native " << error.nativeErrorCode() << ")\n";
}
```

`Input::update()`, `Console::terminalSize()`, and `Console::endFrame()` report
`NoActiveSession` when used without a live `TerminalSession`.

## CMake

As a subdirectory:

```cmake
add_subdirectory(external/terminalTool)
target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

Examples, tests, and documentation default off when terminalTool is a
subproject. They can be enabled explicitly with the `TERMINALTOOL_BUILD_*`
options.

Installed package:

```cmake
find_package(terminalTool 0.3.1 CONFIG REQUIRED)
target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

The library remains static regardless of `BUILD_SHARED_LIBS`.

## Version verification

`Version.h` is generated into the build directory.

```sh
cmake --build build --target terminalToolVersionCheck
```

## Tests and documentation

```sh
cmake -S . -B build -DTERMINALTOOL_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

```sh
cmake -S . -B build -DTERMINALTOOL_BUILD_DOCS=ON
cmake --build build --target terminalToolDocs
```

See `docs/input.md`, `docs/platforms.md`, `docs/clipping.md`,
`docs/error-handling.md`, and `docs/versioning.md`.

## Licence and ownership

terminalTool is distributed under the Mozilla Public License 2.0. See
`LICENSE` and `NOTICE.md`.

Copyright (c) 2026 Ataerk YILDIRIM.
