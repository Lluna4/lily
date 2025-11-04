#include "block_registry_processing.h"
#include "json_reader.h"
#include "log.h"
#include <tuple>
#include <utility>

void process_item_registry(const std::string& path, std::map<int, std::string> &items)
{
	if (!std::filesystem::exists(path))
		return;
	std::ifstream registry(path);

	std::string line;
	int state = 0;
	std::string buf;
	while (std::getline(registry, line))
	{
		if (state == 0)
		{
			bool in_string = false;
			std::string tmp;
			int spaces = 0;
			for (char &letter: line)
			{
				if (letter == ' ')
					spaces++;
				if (spaces > 4)
					break;
				if (letter == '"')
				{
					if (in_string == false)
					{
						in_string = true;
						continue;
					}
					break;
				}
				if (in_string)
					tmp.push_back(letter);
			}
			if (tmp == "minecraft:item")
				state = 1;
		}
		else if (state == 1)
		{
			bool in_string = false;
			std::string tmp;
			int spaces = 0;
			bool brk = false;
			for (auto &letter: line)
			{
				if (spaces == 4)
				{
					if (letter == '}')
					{
						brk = true;
						break;
					}
				}
				if (letter == '"')
				{
					if (in_string == false)
					{
						in_string = true;
						continue;
					}
					break;
				}
				if (in_string)
					tmp.push_back(letter);
				if (letter == ' ' && in_string == false)
					spaces++;
			}
			if (brk == true)
				break;
			if (tmp.starts_with("minecraft"))
			{
				buf = tmp;
			}
			else if (tmp.starts_with("protocol"))
			{
				std::string num;
				bool in_num = false;
				for (auto &letter: line)
				{
					if (in_num == true && isdigit(letter))
					{
						num.push_back(letter);
					}
					if (letter == ':')
						in_num = true;
				}
				items.emplace(atoi(num.c_str()), buf);
				log(std::format("added {}: {}", atoi(num.c_str()), buf), LOG_LEVEL::NORMAL);
				buf.clear();
			}
		}
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
