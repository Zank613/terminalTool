# Migrating from 0.3.0 to 0.3.1

No ordinary drawing or key-state calls were removed.

## Version files

Delete any copied `include/terminalTool/Version.h`. Version metadata is generated
in the build directory. Existing stale files now produce a configure error.

## Input queue

The raw queue is now bounded. The default is 4096 events. Applications that do
not consume raw events no longer grow memory indefinitely. Inspect
`eventOverflowed()` and `droppedEventCount()` when every raw event matters.

## Runtime ordering

Continue calling `TerminalSession::update()` before `Input::update()`. This is
now significant after POSIX resume because update reapplies raw terminal state.

## Errors

Calls requiring a live session now throw `NoActiveSession` rather than touching
an uninitialized backend.

## CMake

When terminalTool is included as a subdirectory, example, test, and docs targets
default off. Enable them explicitly when developing terminalTool itself.
The Doxygen target is now `terminalToolDocs`.
