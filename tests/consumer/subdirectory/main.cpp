#include <terminalTool/terminalTool.h>

int main() {
    tt::DeltaTime timer;
    return timer.seconds() == 0.0 ? 0 : 1;
}
