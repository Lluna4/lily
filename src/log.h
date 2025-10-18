#include <print>
#include <fstream>

enum class LOG_LEVEL
{
  NORMAL,
  WARNING,
  ERROR
};

void log(std::string text, LOG_LEVEL level);
