# Changelog

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
- `Colour::foreground()` and `Colour::background()` now return cached references.
- Frame presentation swaps framebuffers instead of copying the full vectors.
- The CMake library remains explicitly static regardless of `BUILD_SHARED_LIBS`.
- Terminal initialization now fails loudly with documented exceptions.

## 0.1.0

- Initial namespaced terminal framebuffer, drawing, resize, Windows input, and
  RAII terminal-session release.
