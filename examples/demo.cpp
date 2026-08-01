/**
 * @file demo.cpp
 * @brief Demonstrates the terminalTool public API.
 * @example demo.cpp
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
    void removeLastUtf8Character(std::string& text) {
        if (text.empty()) {
            return;
        }

        std::size_t position = text.size() - 1;

        while (position > 0 && (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
            position--;
        }

        text.erase(position);
    }


    void removeFirstUtf8Character(std::string& text) {
        if (text.empty()) {
            return;
        }

        std::size_t byteCount = 1;

        while (byteCount < text.size() && (static_cast<unsigned char>(text[byteCount]) & 0xC0) == 0x80) {
            byteCount++;
        }

        text.erase(0, byteCount);
    }
}

int main() {
    try {
        tt::TerminalSession terminal("terminalTool 0.2.0 Demo");

        constexpr auto frameDuration = std::chrono::milliseconds(16);

    int playerX = 8;
    int playerY = 6;
    bool running = true;
    bool showHelp = true;
    std::string typedText;

    while (running) {
        const auto frameStart = std::chrono::steady_clock::now();

        (void) terminal.update();
        tt::Input::update();

        if (tt::Input::isPressed(tt::Key::Escape)) {
            running = false;
        }

        if (tt::Input::isPressed(tt::Key::Tab)) {
            showHelp = !showHelp;
        }

        if (tt::Input::isPressed(tt::Key::Backspace)) {
            removeLastUtf8Character(typedText);
        }

        typedText += tt::Input::textInput();

        while (typedText.size() > 80) {
            removeFirstUtf8Character(typedText);
        }

        int horizontalMovement = 0;
        int verticalMovement = 0;

        if (tt::Input::isHeld(tt::Key::A) || tt::Input::isHeld(tt::Key::Left)) {
            horizontalMovement--;
        }

        if (tt::Input::isHeld(tt::Key::D) || tt::Input::isHeld(tt::Key::Right)) {
            horizontalMovement++;
        }

        if (tt::Input::isHeld(tt::Key::W) || tt::Input::isHeld(tt::Key::Up)) {
            verticalMovement--;
        }

        if (tt::Input::isHeld(tt::Key::S) || tt::Input::isHeld(tt::Key::Down)) {
            verticalMovement++;
        }

        const int width = tt::Console::getFrameWidth();
        const int height = tt::Console::getFrameHeight();

        const tt::Colour screenBackground = tt::Colours::DefaultBackground;
        const tt::Colour panelBackground(20, 20, 20);
        const tt::Colour text = tt::Colours::BrightWhite;
        const tt::Colour muted = tt::Colours::White;
        const tt::Colour accent = tt::Colours::BrightYellow;
        const tt::Colour playerColour = tt::Colours::BrightCyan;

        tt::Console::beginFrame(text, screenBackground);

        if (width < 58 || height < 18) {
            tt::Console::drawTextAligned(
                tt::Console::Rect { 0, 1, width, 1 },
                "Terminal is too small",
                tt::Console::TextAlignment::Centre,
                accent,
                screenBackground
            );

            tt::Console::drawTextAligned(
                tt::Console::Rect { 0, 3, width, 1 },
                "Resize it to at least 58 x 18",
                tt::Console::TextAlignment::Centre,
                text,
                screenBackground
            );

            tt::Console::endFrame();
            std::this_thread::sleep_until(frameStart + frameDuration);
            continue;
        }

        const int sidebarWidth = showHelp ? 31 : 0;

        const tt::Console::Rect titleArea { 0, 0, width, 1 };
        const tt::Console::Rect worldPanel { 1, 2, width - sidebarWidth - 3, height - 4 };
        const tt::Console::Rect helpPanel { width - sidebarWidth - 1, 2, sidebarWidth, height - 4 };

        playerX += horizontalMovement;
        playerY += verticalMovement;

        playerX = std::clamp(playerX, worldPanel.x + 1, worldPanel.x + worldPanel.width - 2);
        playerY = std::clamp(playerY, worldPanel.y + 1, worldPanel.y + worldPanel.height - 2);

        tt::Console::drawTextAligned(
            titleArea,
            "TERMINALTOOL 0.2.0",
            tt::Console::TextAlignment::Centre,
            accent,
            screenBackground
        );

        tt::Console::drawPanel(
            worldPanel,
            "World",
            tt::Colour(100, 180, 210),
            text,
            panelBackground,
            tt::Console::BoxStyle::Single
        );

        for (int y = worldPanel.y + 2; y < worldPanel.y + worldPanel.height - 1; y += 4) {
            tt::Console::drawHorizontalLine(
                worldPanel.x + 2,
                y,
                worldPanel.width - 4,
                U'·',
                tt::Colour(65, 65, 65),
                panelBackground
            );
        }

        tt::Console::drawCell(playerX, playerY, U'@', playerColour, panelBackground);

        const tt::Console::Rect worldContent {
            worldPanel.x + 2,
            worldPanel.y + 1,
            worldPanel.width - 4,
            worldPanel.height - 2
        };

        const std::string positionText =
            "Position: " +
            std::to_string(playerX) + ", " +
            std::to_string(playerY);

        tt::Console::drawTextClipped(
            worldContent.x,
            worldPanel.y + worldPanel.height - 3,
            positionText,
            worldContent,
            muted,
            panelBackground
        );

        tt::Console::drawTextClipped(
            worldContent.x,
            worldPanel.y + worldPanel.height - 2,
            "Typed: " + typedText,
            worldContent,
            muted,
            panelBackground
        );

        if (showHelp) {
            tt::Console::drawPanel(
                helpPanel,
                "Controls",
                tt::Colour(190, 150, 90),
                text,
                panelBackground,
                tt::Console::BoxStyle::Double
            );

            const tt::Console::Rect helpContent {
                helpPanel.x + 2,
                helpPanel.y + 2,
                helpPanel.width - 4,
                helpPanel.height - 4
            };

            tt::Console::drawWrappedText(
                helpContent,
                "WASD or arrows move the @. TAB toggles this panel. Type normally to test UTF-8 text events. BACKSPACE edits the sample text. ESC exits safely. Ctrl+C also restores the terminal before Windows closes the program.",
                tt::Console::TextAlignment::Left,
                text,
                panelBackground
            );
        }

        const std::string focusText = tt::Input::isFocused() ? "focused" : "not focused";

        tt::Console::drawTextAligned(
            tt::Console::Rect { 0, height - 1, width, 1 },
            "event input • wrapped text • Ctrl+C restoration • " + focusText,
            tt::Console::TextAlignment::Centre,
            muted,
            screenBackground
        );

        tt::Console::endFrame();
        std::this_thread::sleep_until(frameStart + frameDuration);
    }

        return 0;
    } catch (const tt::TerminalError& error) {
        std::cerr
            << "terminalTool initialization failed: " << error.what()
            << " (native error " << error.nativeErrorCode() << ")\n";
        return 1;
    }
}
