/**
 * @file publicHeaderSmoke.cpp
 * @brief Verifies that the complete public API can be included by itself.
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
    const tt::TerminalOptions options { "terminalTool", false };
    const tt::Colour colour = tt::Colours::BrightCyan;
    const tt::Console::Rect rectangle { 0, 0, 10, 5 };
    const tt::DeltaTime deltaTime;

    return options.title.empty() || colour.getBlue() == 0 || !rectangle.contains(0, 0) || deltaTime.seconds() != 0.0;
}
