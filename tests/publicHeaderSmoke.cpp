/**
 * @file publicHeaderSmoke.cpp
 * @brief Verifies that the complete public API can be included alone.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/terminalTool.h"

#ifdef _WIN32
#ifdef min
#error "terminalTool public headers leaked the Windows min macro"
#endif
#ifdef max
#error "terminalTool public headers leaked the Windows max macro"
#endif
#endif

int main() {
    tt::TerminalOptions options;
    options.title = "terminalTool";
    options.enableFocusEvents = true;

    const tt::Console::Rect rectangle { 0, 0, 10, 5 };
    const tt::ModifierState modifiers {};
    const tt::InputEvent event {};

    return
        options.title.empty() ||
        !rectangle.contains(0, 0) ||
        modifiers.shift() ||
        event.key.key != tt::Key::Unknown;
}
