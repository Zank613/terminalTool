/**
 * @file NativeInputEvent.h
 * @brief Internal platform-neutral input records.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <cstdint>

#include "terminalTool/Input.h"

namespace tt::detail {

enum class NativeInputEventType {
    KeyDown,
    KeyUp,
    Text,
    FocusGained,
    FocusLost
};

struct NativeInputEvent {
    NativeInputEventType type = NativeInputEventType::KeyDown;
    Key key = Key::Unknown;
    char32_t character = U'\0';
    bool repeated = false;
    std::uint16_t repeatCount = 1;
    std::uint16_t scanCode = 0;
    std::uint32_t nativeKeyCode = 0;
    ModifierState modifiers {};
};

} // namespace tt::detail
