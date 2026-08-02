# terminalTool 0.2.1

`terminalTool` is a small C++17 terminal rendering and input library for
terminal-based games and interactive tools. Its public API lives in the `tt`
namespace and the library is deliberately built as a static library.

```cpp
#include <terminalTool/terminalTool.h>
```

## 0.2.1 highlights

- Strong exception safety when the terminal framebuffer is resized
- Documented runtime errors for input reads and output writes
- Original Windows console title restoration
- Optional alternate-screen support
- No Windows headers or macros exposed through public headers
- Framebuffer and input lifetime controlled exclusively by `tt::TerminalSession`
- `tt::Console::invalidate()` for forced full redraws
- CMake-generated `tt::Version` constants
- Self-contained unit tests and Windows CI for MSVC and MinGW

The 0.2.0 features remain present: comprehensive Windows keyboard coverage,
single-session ownership, framebuffer swapping, cached ANSI colour sequences,
the built-in `tt::Colours` palette, and documented `tt::TerminalError` values.

## Minimal use

```cpp
#include <iostream>

#include <terminalTool/terminalTool.h>

int main() {
    try {
        const tt::TerminalOptions options {
            "My Game",
            true // Use the alternate screen buffer.
        };

        tt::TerminalSession terminal(options);
        bool running = true;

        while (running) {
            (void) terminal.update();
            tt::Input::update();

            if (tt::Input::isPressed(tt::Key::Escape)) {
                running = false;
            }

            tt::Console::beginFrame(
                tt::Colours::BrightWhite,
                tt::Colours::DefaultBackground
            );

            tt::Console::drawText(
                2,
                2,
                "terminalTool is running",
                tt::Colours::BrightYellow
            );

            tt::Console::endFrame();
        }
    } catch (const tt::TerminalError& error) {
        std::cerr
            << error.what()
            << " (native error " << error.nativeErrorCode() << ")\n";
        return 1;
    }
}
```

## Terminal options

```cpp
const tt::TerminalOptions options {
    "My Game",
    true
};

tt::TerminalSession terminal(options);
```

`alternateScreen = true` enters the terminal's alternate screen buffer. The
normal terminal contents return when the session ends. Set it to `false` when
the rendered output should remain in the normal terminal history.

The original constructor remains available and enables the alternate screen:

```cpp
tt::TerminalSession terminal("My Game");
```

## Lifetime ownership

Exactly one `tt::TerminalSession` may exist in a process. A second construction
throws `tt::TerminalErrorCode::SessionAlreadyActive`.

Framebuffer initialization, framebuffer resizing, framebuffer shutdown, input
initialization, and input reset are internal operations owned by
`TerminalSession`. Applications use only the public drawing, presentation, and
input-query API.

All `TerminalSession`, `Console`, and `Input` calls should be made from the
thread that owns the terminal.

## Runtime errors

Initialization and runtime terminal operations throw `tt::TerminalError`.
Important runtime categories include:

```cpp
tt::TerminalErrorCode::FrameBufferResizeFailed
tt::TerminalErrorCode::FlushInputFailed
tt::TerminalErrorCode::QueryInputEventCountFailed
tt::TerminalErrorCode::ReadInputFailed
tt::TerminalErrorCode::WriteOutputFailed
```

The exception provides:

- `error.code()` — portable terminalTool error category
- `error.nativeErrorCode()` — Windows `GetLastError()` or POSIX `errno` when available
- `error.what()` — human-readable description

## Forced redraws

Call `tt::Console::invalidate()` when external code has written directly to the
terminal or when the physical terminal contents may no longer match the stored
previous framebuffer:

```cpp
if (tt::Input::isPressed(tt::Key::F5)) {
    tt::Console::invalidate();
}
```

The next `tt::Console::endFrame()` performs a complete redraw.

## Framebuffer resizing

`tt::TerminalSession::update()` checks the visible terminal size and safely
replaces both framebuffers when it changes:

```cpp
if (terminal.update()) {
    // Recalculate cached game layouts here.
}
```

The new buffers are allocated before any live dimensions or storage are
changed. If allocation fails, the old framebuffer remains valid and a
`FrameBufferResizeFailed` exception is thrown.

## Keyboard input

Call `tt::Input::update()` exactly once near the start of every frame.

```cpp
if (tt::Input::isPressed(tt::Key::F1)) {
    openHelp();
}

if (tt::Input::isHeld(tt::Key::LeftShift) &&
    tt::Input::isHeld(tt::Key::W)) {
    sprintForward();
}

if (tt::Input::isReleased(tt::Key::Space)) {
    releaseChargedAction();
}
```

The Windows backend covers letters, number-row keys, F1-F24, navigation,
left/right modifiers, punctuation, the complete numpad, IME/international keys,
browser/media controls, application launch keys, and Windows legacy keys.

Printable text is exposed separately as UTF-8:

```cpp
std::string command;
command += tt::Input::textInput();
```

## Built-in colours

Every unique RGB value shares cached foreground and background ANSI strings.
A conventional 16-colour RGB palette is available under `tt::Colours`:

```cpp
tt::Colours::Red
tt::Colours::BrightYellow
tt::Colours::BrightCyan
tt::Colours::DefaultForeground
tt::Colours::DefaultBackground
```

Custom true colours remain available:

```cpp
const tt::Colour sugarGold(235, 190, 90);
```

## CMake target

The exported and in-tree target is:

```cmake
target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

It remains a static library even when a parent project sets
`BUILD_SHARED_LIBS` to `ON`.

### Use with `add_subdirectory`

```cmake
set(TERMINALTOOL_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TERMINALTOOL_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(TERMINALTOOL_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/terminalTool)
target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

### Install and use with `find_package`

```sh
cmake -S . -B build -DTERMINALTOOL_BUILD_EXAMPLE=OFF
cmake --build build
cmake --install build --prefix install
```

A consuming project can then use:

```cmake
find_package(terminalTool 0.2.1 CONFIG REQUIRED)
target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

## Tests

The project includes self-contained tests that do not require an interactive
terminal:

```sh
cmake -S . -B build -DTERMINALTOOL_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The Windows workflow builds, tests, and installs the project with both MSVC and
MinGW UCRT64.

## Doxygen documentation

Every public class, structure, enum, function, and constant is documented. When
Doxygen is installed, CMake creates a `docs` target:

```sh
cmake -S . -B build
cmake --build build --target docs
```

The generated entry page is `build/documentation/html/index.html`.

## Version

`Version.h` is generated from the CMake project version:

```cpp
static_assert(tt::Version::Major == 0);
static_assert(tt::Version::Minor == 2);
static_assert(tt::Version::Patch == 1);
```

## Platform status

- Windows has complete event-based keyboard input and emergency restoration.
- Rendering and terminal-size queries build on POSIX systems.
- Non-Windows keyboard input is still intentionally inactive.
- Unicode code points are currently treated as one terminal cell each. Wide
  glyphs and combining sequences require a future display-width implementation.

## Licence and ownership

terminalTool is distributed under the **Mozilla Public License 2.0**. See
`LICENSE` for the complete terms and `NOTICE.md` for the project copyright
notice.

Copyright (c) 2026 Ataerk YILDIRIM.
