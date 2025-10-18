#include "log.h"

void log(std::string text, LOG_LEVEL level)
{
    std::ofstream l("log.txt", std::ios_base::app);
    l << text << '\n';
    if (level == LOG_LEVEL::NORMAL)
        std::println("{}", text);
    else if (level == LOG_LEVEL::WARNING)
        std::println("\x1b[1;33m{}\033[0m", text);
    else if (level == LOG_LEVEL::ERROR)
        std::println("\x1b[1;31m{}\033[0m", text);
}
