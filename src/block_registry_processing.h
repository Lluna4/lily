#pragma once
#include <map>
#include <string>
#include <filesystem>
#include <fstream>
#include <print>

void process_block_registry(const std::string& path, std::map<int, std::string> &blocks);