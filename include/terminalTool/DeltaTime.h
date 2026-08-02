/**
 * @file DeltaTime.h
 * @brief Declares a small steady-clock delta-time utility.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#pragma once

#include <chrono>

namespace tt {

/**
 * @brief Measures elapsed real time between successive frame updates.
 *
 * DeltaTime uses `std::chrono::steady_clock`, so its measurements are not
 * affected by changes to the system wall clock. It does not sleep, limit the
 * frame rate, or own a game loop; applications remain responsible for those
 * policies.
 *
 * Call update() once near the beginning of each frame, then use seconds() or
 * milliseconds() while updating frame-rate-independent game state.
 */
class DeltaTime {
private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point previousUpdate;
    std::chrono::duration<double> elapsed {};

public:
    /**
     * @brief Starts the timer with a stored delta of zero.
     */
    DeltaTime() noexcept;

    /**
     * @brief Measures the time elapsed since construction, reset(), or update().
     * @return The newly measured delta time in seconds.
     */
    double update() noexcept;

    /**
     * @brief Restarts measurement and clears the stored delta time.
     */
    void reset() noexcept;

    /**
     * @return The most recently measured delta time in seconds.
     */
    [[nodiscard]] double seconds() const noexcept;

    /**
     * @return The most recently measured delta time in milliseconds.
     */
    [[nodiscard]] double milliseconds() const noexcept;
};

} // namespace tt
