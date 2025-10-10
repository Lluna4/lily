#pragma once
#include <map>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <print>
#include "json_reader.h"

struct block_state
{
	int id;
	std::map<std::string, std::string> properties;
};

void process_item_registry(const std::string& path, std::map<int, std::string> &items);
json_value process_block_registry(const std::string& path);
