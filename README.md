# terminalTool 0.2.0

`terminalTool` is a small C++17 terminal rendering and input library for
terminal-based games and interactive tools. Its public API lives in the `tt`
namespace and the library is deliberately built as a static library.

```cpp
#include <terminalTool/terminalTool.h>
```

## 0.2.0 highlights

- Comprehensive Windows console keyboard coverage
- Documented `tt::TerminalError` exceptions with portable and native error codes
- Exactly one `tt::TerminalSession` per process
- Static-library-only CMake target
- Framebuffer swapping instead of whole-frame copying
- Globally cached ANSI true-colour sequences
- Ready-to-use conventional ANSI/VGA-style colours under `tt::Colours`
- MPL 2.0 licensing, copyright notice, and source-file SPDX headers
- Public semantic version constants under `tt::Version`

## Minimal use

```cpp
#include <iostream>

#include <terminalTool/terminalTool.h>

int main() {
    try {
        tt::TerminalSession terminal("My Game");
        bool running = true;

        while (running) {
            terminal.update();
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

## Library target

The exported and in-tree CMake target is:

```cmake
target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

The target remains a static library even when a parent project sets
`BUILD_SHARED_LIBS` to `ON`.

## Use with `add_subdirectory`

Place the project under your own repository, for example:

```text
MyGame/
├── CMakeLists.txt
├── src/
└── external/
    └── terminalTool/
```

Then link it:

```cmake
set(TERMINALTOOL_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TERMINALTOOL_BUILD_DOCS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/terminalTool)

target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

## Install and use with `find_package`

```sh
cmake -S . -B build -DTERMINALTOOL_BUILD_EXAMPLE=OFF
cmake --build build
cmake --install build --prefix install
```

A consuming project can then use:

```cmake
find_package(terminalTool 0.2.0 CONFIG REQUIRED)
target_link_libraries(MyGame PRIVATE terminalTool::terminalTool)
```

Pass the installation directory through `CMAKE_PREFIX_PATH` while configuring
the consuming project.

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

The Windows backend covers:

- `A-Z` and number-row `0-9`
- F1-F24
- arrows, Home, End, Insert, Delete, Page Up, and Page Down
- left/right Shift, Control, Alt, and Windows keys
- punctuation and non-US backslash keys
- the complete numpad, including distinct numpad Enter
- Caps Lock, Num Lock, Scroll Lock, Print Screen, Pause, Menu, and Sleep
- IME and international keyboard keys exposed by the Windows console
- browser, volume, media, and application-launch keys
- Windows legacy keyboard virtual keys

Printable text is exposed separately as UTF-8:

```cpp
std::string command;
command += tt::Input::textInput();
```

## Initialization errors

`tt::TerminalSession` throws `tt::TerminalError` when initialization cannot be
completed. The exception contains:

- `error.code()` — a portable `tt::TerminalErrorCode`
- `error.nativeErrorCode()` — `GetLastError()` on Windows or `errno` on POSIX
- `error.what()` — a human-readable explanation

Examples include redirected standard streams, invalid console handles, ANSI
mode failure, UTF-8 code-page failure, control-handler failure, terminal-size
query failure, and trying to create a second session.

```cpp
try {
    tt::TerminalSession first;
    tt::TerminalSession second; // Throws SessionAlreadyActive.
} catch (const tt::TerminalError& error) {
    if (error.code() == tt::TerminalErrorCode::SessionAlreadyActive) {
        // Handle the programming error.
    }
}
```

## Built-in colours

Every unique RGB value shares cached foreground and background ANSI strings.
`Colour::foreground()` and `Colour::background()` return references to these
cached strings.

A conventional 16-colour RGB palette is available instantly:

```cpp
tt::Colours::Black
tt::Colours::Red
tt::Colours::Green
tt::Colours::Yellow
tt::Colours::Blue
tt::Colours::Magenta
tt::Colours::Cyan
tt::Colours::White

tt::Colours::BrightBlack
tt::Colours::BrightRed
tt::Colours::BrightGreen
tt::Colours::BrightYellow
tt::Colours::BrightBlue
tt::Colours::BrightMagenta
tt::Colours::BrightCyan
tt::Colours::BrightWhite
```

Also provided:

```cpp
tt::Colours::Grey
tt::Colours::LightGrey
tt::Colours::DefaultForeground
tt::Colours::DefaultBackground
```

Custom true colours remain available:

```cpp
const tt::Colour sugarGold(235, 190, 90);
```

## Framebuffer behavior

The frame loop remains:

```cpp
tt::Console::beginFrame();
// Draw the complete desired frame.
tt::Console::endFrame();
```

`endFrame()` compares the new frame against the previously presented frame,
selects a full or differential update, and swaps the two framebuffer vectors.
It does not copy the complete framebuffer after each presented frame.

## Doxygen documentation

Every public class, enum, function, and constant is documented. When Doxygen is
installed, CMake creates a `docs` target:

```sh
cmake -S . -B build
cmake --build build --target docs
```

The generated entry page is:

```text
build/documentation/html/index.html
```

## Demo

Open the extracted project in CLion, select `terminalToolDemo`, and run the
resulting executable in Windows Terminal or another real console.

- `WASD` or arrow keys move the `@`
- printable typing tests UTF-8 text events
- Backspace edits the sample text
- Tab toggles the help panel
- Escape exits normally
- Ctrl+C tests emergency terminal restoration

CLion's normal output window may not provide real console events, resizing, or
ANSI behavior.

## Version

```cpp
static_assert(tt::Version::Major == 0);
static_assert(tt::Version::Minor == 2);
static_assert(tt::Version::Patch == 0);
```

## Platform status

- Windows has the complete event-based keyboard implementation.
- Rendering and terminal-size queries build on POSIX systems.
- Non-Windows keyboard input is still intentionally inactive.
- Unicode code points are currently treated as one terminal cell each. Wide
  glyphs and combining sequences need a future display-width implementation.

## Licence and ownership

terminalTool is distributed under the **Mozilla Public License 2.0**. See
`LICENSE` for the complete terms and `NOTICE.md` for the project copyright
notice.

The MPL 2.0 is a file-level weak-copyleft licence. Covered source files and
modifications to them must remain available under the MPL when distributed,
while separate files in a larger application may use other terms. This allows
terminalTool to be linked into open or proprietary games without relicensing
the game's separate source files.

Copyright (c) 2026 Ataerk YILDIRIM.
