#include "block_registry_processing.h"
#include "json_reader.h"
#include "log.h"
#include <tuple>
#include <utility>

void process_item_registry(const std::string& path, std::unordered_map<int, std::string> &items)
{
	if (!std::filesystem::exists(path))
		return;
	std::ifstream registry(path);
	std::stringstream buffer;
	buffer << registry.rdbuf();
	std::string d = buffer.str();
	json_parser p((char *)d.c_str());
	registry.close();
	json_value val = p.parse();

	for (auto &item: val.get<json_object>()["minecraft:item"].get<json_object>()["entries"].get<json_object>())
	{
		items.emplace(std::piecewise_construct, std::forward_as_tuple(item.second.get<json_object>()["protocol_id"].get<long>()), std::forward_as_tuple(item.first));
	}

	registry.close();
}

std::map<std::string, block> process_block_registry(const std::string& path)
{
	std::map<std::string, block> blocks;
	if (!std::filesystem::exists(path))
		return blocks;
	std::ifstream registry(path);
	std::stringstream buffer;
	buffer << registry.rdbuf();
	std::string d = buffer.str();
	json_parser p((char *)d.c_str());
	registry.close();
	json_value val = p.parse();
	
	for (auto &[key, val]: val.get<json_object>())
	{
		blocks.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(val));
	}

	return blocks;
}
