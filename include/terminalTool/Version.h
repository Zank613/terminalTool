/**
 * @file Version.h
 * @brief Provides terminalTool version constants.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

namespace tt {

/** @brief terminalTool semantic version information. */
namespace Version {
    inline constexpr int Major = 0; ///< Major version component.
    inline constexpr int Minor = 2; ///< Minor version component.
    inline constexpr int Patch = 0; ///< Patch version component.
    inline constexpr const char* String = "0.2.0"; ///< Complete semantic version.
}

} // namespace tt
