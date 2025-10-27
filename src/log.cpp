#include "log.h"

std::string log_file;

void log(std::string text, LOG_LEVEL level)
{
    std::ofstream l(log_file, std::ios_base::app);
    l << text << '\n';
    if (level == LOG_LEVEL::NORMAL)
        std::println("{}", text);
    else if (level == LOG_LEVEL::WARNING)
        std::println("\x1b[1;33m{}\033[0m", text);
    else if (level == LOG_LEVEL::ERROR)
        std::println("\x1b[1;31m{}\033[0m", text);
}

bool create_log_file()
{
    bool ret = true;
    if (!std::filesystem::exists("logs"))
		ret = std::filesystem::create_directory("logs");
    if (ret == false)
        return ret;
	log_file = std::format("logs/log_{:%Od-%Om-%Oy_%OH-%OM-%OS}.txt", std::chrono::system_clock::now());
    return ret;
}
