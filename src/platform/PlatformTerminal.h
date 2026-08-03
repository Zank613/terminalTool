/**
 * @file PlatformTerminal.h
 * @brief Internal terminal platform abstraction.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <string>
#include <vector>

#include "detail/NativeInputEvent.h"
#include "terminalTool/TerminalSession.h"

namespace tt::detail {

struct TerminalDimensions {
    int width = 0;
    int height = 0;
};

void platformInitialize(const TerminalOptions& options);
void platformShutdown() noexcept;
[[nodiscard]] TerminalDimensions platformTerminalSize();
void platformWriteOutput(const std::string& output);
void platformFlushInput();
void platformReadInput(std::vector<NativeInputEvent>& events);

} // namespace tt::detail
