/**
 * @file demo.cpp
 * @brief Demonstrates terminalTool 0.3.0 input events and clipping.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "terminalTool/terminalTool.h"

namespace {

std::string eventName(const tt::InputEventType type) {
    switch (type) {
        case tt::InputEventType::KeyPressed: return "KeyPressed";
        case tt::InputEventType::KeyReleased: return "KeyReleased";
        case tt::InputEventType::TextEntered: return "TextEntered";
        case tt::InputEventType::FocusGained: return "FocusGained";
        case tt::InputEventType::FocusLost: return "FocusLost";
    }
    return "Unknown";
}

} // namespace

int main() {
    try {
        tt::TerminalOptions options;
        options.title = "terminalTool 0.3.0 demo";
        options.alternateScreen = true;
        options.enableFocusEvents = true;

        tt::TerminalSession terminal(options);
        tt::DeltaTime deltaTime;

        double playerX = 4.0;
        double playerY = 4.0;
        std::string typedText;
        std::string latestEvent = "No event yet";
        bool running = true;

        while (running) {
            const auto frameStart = std::chrono::steady_clock::now();
            const double seconds = std::min(deltaTime.update(), 0.1);

            (void) terminal.update();
            tt::Input::update();

            if (tt::Input::isPressed(tt::Key::Escape)) {
                running = false;
            }

            const double speed = 18.0;
            const auto active = [](const tt::Key key) {
                return tt::Input::isHeld(key) || tt::Input::isPressed(key);
            };

            if (active(tt::Key::W) || active(tt::Key::Up)) playerY -= speed * seconds;
            if (active(tt::Key::S) || active(tt::Key::Down)) playerY += speed * seconds;
            if (active(tt::Key::A) || active(tt::Key::Left)) playerX -= speed * seconds;
            if (active(tt::Key::D) || active(tt::Key::Right)) playerX += speed * seconds;

            typedText += tt::Input::textInput();
            if (tt::Input::isPressed(tt::Key::Backspace) && !typedText.empty()) {
                typedText.pop_back();
            }

            while (const auto event = tt::Input::pollEvent()) {
                latestEvent = eventName(event->type);
                if (event->type == tt::InputEventType::KeyPressed) {
                    latestEvent += event->key.repeated ? " (repeat)" : "";
                }
            }

            const int width = tt::Console::getFrameWidth();
            const int height = tt::Console::getFrameHeight();
            playerX = std::clamp(playerX, 2.0, static_cast<double>(std::max(2, width - 3)));
            playerY = std::clamp(playerY, 3.0, static_cast<double>(std::max(3, height - 6)));

            tt::Console::beginFrame(tt::Colours::BrightWhite, tt::Colours::DefaultBackground);
            tt::Console::drawPanel(
                { 0, 0, width, height },
                " terminalTool 0.3.0 ",
                tt::Colours::BrightCyan,
                tt::Colours::BrightWhite,
                tt::Colours::DefaultBackground,
                tt::Console::BoxStyle::Double
            );

            const tt::Console::Rect world { 2, 2, std::max(0, width - 4), std::max(0, height - 7) };
            {
                tt::Console::ScopedClip clip(world);
                tt::Console::fillRect(world, U'.', tt::Colour(50, 70, 70), tt::Colours::DefaultBackground);
                tt::Console::drawCell(
                    static_cast<int>(playerX),
                    static_cast<int>(playerY),
                    U'@',
                    tt::Colours::BrightYellow,
                    tt::Colours::DefaultBackground
                );
            }

            tt::Console::drawText(2, height - 4, "WASD/arrows move | Escape exits", tt::Colours::BrightGreen);
            tt::Console::drawText(2, height - 3, "Focus: " + std::string(tt::Input::isFocused() ? "yes" : "no") + " | " + latestEvent);
            tt::Console::drawTextClipped(2, height - 2, "Typed: " + typedText, { 2, height - 2, std::max(0, width - 4), 1 });
            tt::Console::endFrame();

            std::this_thread::sleep_until(frameStart + std::chrono::milliseconds(16));
        }
    } catch (const tt::TerminalError& error) {
        std::cerr << error.what() << " (native error " << error.nativeErrorCode() << ")\n";
        return 1;
    }

    return 0;
}
