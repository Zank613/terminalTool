# Changelog

## 0.3.0

- Added a persistent FIFO `tt::InputEvent` queue.
- Added rich key metadata: repeat state/count, scan code, native key code, and modifiers.
- Added complete left/right modifier, lock-key, Super, AltGr, and enhanced-key state.
- Renamed punctuation keys to layout-neutral OEM keys while retaining 0.2.x aliases.
- Added focus gained/lost events and per-frame focus transition queries.
- Expanded `tt::TerminalOptions` with cursor, wrapping, focus, clear, signal, and title controls.
- Added Linux and macOS raw, non-blocking keyboard backends.
- Added POSIX signal restoration and SIGWINCH observation.
- Split platform code behind an internal terminal abstraction.
- Added a shared incremental UTF-8, CSI, and SS3 escape-sequence parser.
- Added nested clipping regions and `tt::Console::ScopedClip`.
- Expanded unit tests, parser tests, CI, Doxygen comments, and guides.

## 0.2.2

- Added `tt::DeltaTime` based on `std::chrono::steady_clock`.

## 0.2.1

- Hardened resize allocation, error reporting, restoration, lifecycle ownership, generated versioning, tests, and Windows CI.

## 0.2.0

- Added broad Windows keyboard coverage, `tt::TerminalError`, static-only packaging, framebuffer swapping, cached ANSI colours, and MPL-2.0 licensing.
