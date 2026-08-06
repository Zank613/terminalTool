/**
 * @file posixSessionProbe.cpp
 * @brief Interactive helper used by POSIX pseudo-terminal integration tests.
 *
 * SPDX-License-Identifier: MPL-2.0
 * Copyright (c) 2026 Ataerk YILDIRIM
 */

#include <chrono>
#include <csignal>
#include <string>
#include <thread>

#include <unistd.h>

#include "terminalTool/terminalTool.h"

namespace {

volatile std::sig_atomic_t previousHandlerReturned = 0;

void previousTermHandler(int) {
    _exit(77);
}

void returningTermHandler(int) {
    previousHandlerReturned = 1;
}

} // namespace

int main(const int argc, char** argv) {
    std::signal(SIGTSTP, SIG_DFL);
    std::signal(SIGCONT, SIG_DFL);

    if (argc > 1 && std::string(argv[1]) == "--previous-term-handler") {
        std::signal(SIGTERM, previousTermHandler);
    } else if (argc > 1 && std::string(argv[1]) == "--returning-term-handler") {
        std::signal(SIGTERM, returningTermHandler);
    }

    tt::TerminalOptions options;
    options.title = "terminalTool probe\033]0;injected\007";
    options.alternateScreen = true;
    options.installSignalHandlers = true;
    options.enableFocusEvents = true;

    tt::TerminalSession terminal(options);
    bool running = true;

    while (running) {
        (void) terminal.update();
        tt::Input::update();
        if (tt::Input::isPressed(tt::Key::Escape)) {
            running = false;
        }

        tt::Console::beginFrame();
        tt::Console::drawText(0, 0, previousHandlerReturned != 0
            ? "terminalTool POSIX probe: previous handler returned"
            : "terminalTool POSIX probe");
        tt::Console::endFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return 0;
}
