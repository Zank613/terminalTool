/**
 * @file DeltaTime.cpp
 * @brief Implements the terminalTool delta-time utility.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/DeltaTime.h"

namespace tt {

DeltaTime::DeltaTime() noexcept
    : previousUpdate(Clock::now()) {}

double DeltaTime::update() noexcept {
    const Clock::time_point currentUpdate = Clock::now();
    elapsed = currentUpdate - previousUpdate;
    previousUpdate = currentUpdate;
    return elapsed.count();
}

void DeltaTime::reset() noexcept {
    previousUpdate = Clock::now();
    elapsed = std::chrono::duration<double>::zero();
}

double DeltaTime::seconds() const noexcept {
    return elapsed.count();
}

double DeltaTime::milliseconds() const noexcept {
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

} // namespace tt
