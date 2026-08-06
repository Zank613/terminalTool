# Generated version metadata

The only release-number source is:

```cmake
project(terminalTool VERSION 0.3.1 LANGUAGES CXX)
```

CMake generates `build/generated/terminalTool/Version.h`, then immediately runs
a generated verification script. The same script is exposed through:

```sh
cmake --build build --target terminalToolVersionCheck
```

and CTest as `terminalToolGeneratedVersion`.

GitHub Actions invokes that target and contains no literal terminalTool version.
C++ tests receive expected values from CMake compile definitions, which catches
a stale header being selected by include ordering.

A file at `include/terminalTool/Version.h` is considered stale and causes a
clear configure-time failure. Delete it and clear the build directory. Only
`Version.h.in` belongs in source control.
