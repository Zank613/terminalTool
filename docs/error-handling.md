# Error handling

`tt::TerminalError` contains a portable `TerminalErrorCode` and an optional
native Windows error or POSIX `errno` value.

`NoActiveSession` is reported by runtime operations which require ownership of
an initialized terminal:

- `tt::Input::update()`
- `tt::Console::terminalSize()`
- `tt::Console::endFrame()`
- `tt::TerminalSession::update()` after an inactive state

Drawing calls remain harmless no-ops before framebuffer initialization so
objects can prepare UI state before a session exists.

Input/output and resize failures preserve their documented operation-specific
codes. Framebuffer resize allocation remains strongly exception-safe: existing
buffers and dimensions are retained unless both new buffers are allocated.
