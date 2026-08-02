# Changelog

## 0.2.1 - 2026-08-02

### Added

- `tt::TerminalOptions` with optional alternate-screen behavior.
- Restoration of the original Windows console title.
- Runtime `TerminalErrorCode` values for input flushing, event queries, input
  reads, terminal output writes, and framebuffer resize allocation.
- `tt::Console::invalidate()` to force the next complete redraw.
- Self-contained unit tests and a public-header smoke test.
- Windows GitHub Actions jobs for MSVC and MinGW UCRT64.
- A project `.clang-format` configuration.

### Changed

- Framebuffer resize now provides strong exception safety: temporary buffers are
  allocated before live dimensions or storage are changed.
- Framebuffer allocation failures are translated into documented
  `tt::TerminalError` exceptions.
- Console output failures and Windows input failures are no longer silently
  ignored.
- Framebuffer and input lifetime functions are private and owned by
  `tt::TerminalSession`.
- Windows implementation details are hidden behind a private implementation;
  public headers no longer include `windows.h` or export Windows macros.
- `tt::Version` is generated from the CMake project version.
- The example now demonstrates alternate-screen configuration and forced redraw.

## 0.2.0 - 2026-08-02

### Added

- Comprehensive Windows console keyboard enumeration and mapping.
- `tt::TerminalError` and `tt::TerminalErrorCode`.
- Native operating-system error values on initialization failures.
- Single-session enforcement through `tt::TerminalSession`.
- Built-in `tt::Colours` ANSI/VGA-style RGB palette.
- Cached foreground and background ANSI sequences for each unique RGB colour.
- `tt::Version` constants.
- MPL 2.0 `LICENSE`, `NOTICE.md`, and SPDX source notices.

### Changed

- Project version changed to 0.2.0.
- `Colour::foreground()` and `Colour::background()` return cached references.
- Frame presentation swaps framebuffers instead of copying the full vectors.
- The CMake library remains explicitly static regardless of `BUILD_SHARED_LIBS`.
- Terminal initialization fails loudly with documented exceptions.

## 0.1.0

- Initial namespaced terminal framebuffer, drawing, resize, Windows input, and
  RAII terminal-session release.
