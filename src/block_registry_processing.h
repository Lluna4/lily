#pragma once
#include <map>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <print>
#include "log.h"
#include "json_reader.h"
#include "blocks.h"

struct block_state
{
	int id;
	std::map<std::string, std::string> properties;
};

void process_item_registry(const std::string& path, std::map<int, std::string> &items);
std::map<std::string, block> process_block_registry(const std::string& path);
