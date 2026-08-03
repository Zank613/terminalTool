# Platform support

| Capability | Windows | Linux | macOS |
|---|---:|---:|---:|
| ANSI framebuffer rendering | Yes | Yes | Yes |
| True-colour output | Yes | Terminal-dependent | Terminal-dependent |
| Real key-down and key-up events | Yes | No, pulse model | No, pulse model |
| Raw input event queue | Yes | Yes | Yes |
| Native repeat count | Yes | Not generally available | Not generally available |
| Native scan code | Yes | Not generally available | Not generally available |
| Focus reporting | Console events | CSI I/O when supported | CSI I/O when supported |
| Alternate screen | Yes | Yes | Yes |
| Emergency restoration | Console control handler | POSIX signals | POSIX signals |

## Windows backend

The Windows backend uses `ReadConsoleInputW`, preserves and restores console
modes and UTF-8 code pages, restores the previous title when requested, and
maps Windows virtual keys to layout-neutral terminalTool keys.

## POSIX backend

Linux and macOS share one `termios` backend. Input is raw and non-blocking. The
incremental parser handles UTF-8, ASCII controls, CSI navigation/function-key
sequences, SS3 sequences, xterm modifier parameters, Alt-prefixed text, and
focus events.

SIGINT, SIGTERM, SIGHUP, and SIGQUIT receive best-effort terminal restoration
before default signal handling resumes. SIGWINCH is recorded for terminal size
changes; applications continue calling `TerminalSession::update()` explicitly.

Terminal protocols vary. Unknown escape sequences are safely ignored rather
than guessed into unrelated keys.
