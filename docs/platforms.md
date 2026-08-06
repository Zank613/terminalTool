# Platform support

| Capability | Windows | Linux | macOS |
|---|---:|---:|---:|
| ANSI framebuffer rendering | Yes | Yes | Yes |
| True-colour output | Yes | Terminal-dependent | Terminal-dependent |
| Real key-down and key-up events | Yes | Pulse model | Pulse model |
| Bounded raw input event queue | Yes | Yes | Yes |
| Native repeat count and scan code | Yes | Usually unavailable | Usually unavailable |
| Focus reporting | Console records | CSI I/O when supported | CSI I/O when supported |
| Alternate screen | Yes | Yes | Yes |
| Ctrl+C/termination restoration | Control handler | POSIX signals | POSIX signals |
| Suspend/resume restoration | N/A | SIGTSTP/SIGCONT | SIGTSTP/SIGCONT |
| Resize notification | Polled console size | SIGWINCH | SIGWINCH |

## Windows

The backend uses `ReadConsoleInputW`. Modifier state is tracked in event order
instead of consulting global asynchronous key state. Left/right modifiers,
lock state, enhanced-key state, repeat counts, scan codes, and native virtual
keys are retained.

UTF-16 high-surrogate repeat counts are paired with repeated low surrogates so
supplementary characters do not produce spurious U+FFFD events. Emergency
restoration is guarded so normal destruction and a control handler cannot both
restore the console concurrently.

## POSIX

Linux and macOS share raw non-blocking `termios` input and one strict UTF-8,
CSI, and SS3 parser.

`SIGWINCH` controls when terminal size is queried. If signal handlers are
explicitly disabled, size polling occurs every `TerminalSession::update()`.

Before `SIGTSTP`, terminal attributes, cursor, wrapping, focus reporting, and
the alternate screen are restored. After `SIGCONT`, raw state and configured
terminal modes are reapplied, the framebuffer is invalidated, and size is
rechecked. Existing signal actions are restored and dispatched rather than
silently replaced. If an existing termination handler returns, terminalTool
reapplies its runtime state on the next `TerminalSession::update()`.

POSIX title text is sanitized before being placed in an OSC title sequence.
The previous POSIX title cannot be queried reliably and therefore cannot be
restored exactly.
