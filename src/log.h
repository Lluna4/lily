#pragma once

#include <filesystem>
#include <print>
#include <fstream>
#include <expected>
#include <chrono>

enum class LOG_LEVEL
{
  NORMAL,
  WARNING,
  ERROR
};

bool create_log_file();
void log(std::string text, LOG_LEVEL level);
