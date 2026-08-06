# Changelog

## 0.3.1

- Removed all hard-coded version assertions from GitHub Actions.
- Added CMake-generated version macros, configure-time verification, a reusable
  `terminalToolVersionCheck` target, and a CTest version check.
- Added a source-tree guard that reports stale copied `Version.h` files clearly.
- Added `TerminalErrorCode::NoActiveSession` and runtime session checks.
- Added a bounded raw `InputEvent` queue with overflow reporting.
- Reused native input buffers instead of allocating them every frame.
- Added strict shared UTF-8 decoding/encoding and malformed-input recovery.
- Sanitized framebuffer control characters and POSIX OSC terminal titles.
- Bounded malformed/incomplete escape sequences and replaced `atoi` parsing.
- Added shifted-number and additional Ctrl-key mappings on POSIX.
- Added safe rectangle arithmetic and bounded line drawing.
- Added POSIX `SIGTSTP`/`SIGCONT` restoration and resume handling.
- Preserved and chained pre-existing POSIX signal actions, including handlers that return.
- Used `SIGWINCH` to avoid unnecessary POSIX size polling.
- Added race-resistant Windows emergency restoration.
- Improved Windows modifier snapshots and moved repeated surrogate-pair handling into a tested shared decoder.
- Made development targets default off when used through `add_subdirectory()`.
- Integrated `BUILD_TESTING` and renamed the Doxygen target to
  `terminalToolDocs`.
- Added installed-package, subdirectory-consumer, parser, Unicode, queue,
  pseudo-terminal, suspend/resume, signal-chaining, and sanitizer tests.

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
