#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

const std::string& CONSOLE_LOGGER("console");

int main() {
    auto consoleLogger = spdlog::stdout_color_mt(CONSOLE_LOGGER);
    consoleLogger -> info("Hello, world!");
    return 0;
}