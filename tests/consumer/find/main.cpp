#include <string>
#include <terminalTool/terminalTool.h>

int main() {
    return std::string(tt::Version::String).empty() ? 1 : 0;
}
