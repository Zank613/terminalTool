/**
 * @file Colour.cpp
 * @brief Implements cached ANSI sequences for tt::Colour.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include "terminalTool/Colour.h"

#include <map>
#include <mutex>
#include <utility>

namespace {

struct ColourSequences {
    std::string foreground;
    std::string background;
};

std::uint32_t packedColour(const tt::Colour& colour) {
    return
        static_cast<std::uint32_t>(colour.getRed()) << 16U |
        static_cast<std::uint32_t>(colour.getGreen()) << 8U |
        static_cast<std::uint32_t>(colour.getBlue());
}

const ColourSequences& sequencesFor(const tt::Colour& colour) {
    static std::map<std::uint32_t, ColourSequences> cache;
    static std::mutex cacheMutex;

    const std::uint32_t key = packedColour(colour);
    const std::lock_guard<std::mutex> lock(cacheMutex);

    const auto existing = cache.find(key);

    if (existing != cache.end()) {
        return existing->second;
    }

    const std::string channels =
        std::to_string(colour.getRed()) + ";" +
        std::to_string(colour.getGreen()) + ";" +
        std::to_string(colour.getBlue()) + "m";

    const auto inserted = cache.emplace(
        key,
        ColourSequences {
            "\033[38;2;" + channels,
            "\033[48;2;" + channels
        }
    );

    return inserted.first->second;
}

} // namespace

namespace tt {

const std::string& Colour::foreground() const {
    return sequencesFor(*this).foreground;
}

const std::string& Colour::background() const {
    return sequencesFor(*this).background;
}

} // namespace tt
