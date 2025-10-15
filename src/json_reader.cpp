#include "json_reader.h"

json_value json_parser::parse()
{
	while (data[index] != '\0')
	{
		if (data[index] == '{')
		{
			index++;
			json_value ret{};
			ret.value = parse_object();
			return ret;
		}
		if (data[index] == '[')
		{
			index++;
			json_value ret;
			ret.value = parse_array();
			return ret;
		}
		if (data[index] == '"')
		{
			index++;
			json_value ret;
			ret.value = parse_string();
			return ret;
		}
		if (isdigit(data[index]) != 0)
		{
			json_value ret;
			ret.value = parse_number();
			return ret;
		}
		if (data[index] == 't' || data[index] == 'f')
		{
			json_value ret;
			ret.value = parse_bool();
			return ret;
		}
		index++;
	}
	return json_value{};
}

std::string json_parser::parse_string()
{
	std::string ret;
	while (data[index] != '"')
	{
		ret.push_back(data[index]);
		index++;
	}
	index++;
	return ret;
}

std::map<std::string, json_value> json_parser::parse_object()
{
	std::map<std::string, json_value> ret;
	while (true)
	{
		bool brk2 = false;
		while (data[index] != '"')
		{
			if (data[index] == '}')
			{
				brk2 = true;
				break;
			}
			index++;
		}
		if (brk2 == false)
		{
			index++;
			std::string key = parse_string();

			while (data[index] != ':')
				index++;
			index++;
			//std::println("Getting value with key {}", key);
			json_value value = parse();
			ret.emplace(std::piecewise_construct, std::forward_as_tuple(key),
				std::forward_as_tuple(value.value));
		}
		bool brk = false;
		while (true)
		{
			if (data[index] == ',')
			{
				index++;
				break;
			}
			if (data[index] == '}')
			{
				brk = true;
				index++;
				//std::println("Found object end");
				break;
			}
			index++;
		}
		if (brk == true)
			break;
	}
	return ret;
}

long json_parser::parse_number()
{
	int len = 0;
	while (isdigit(data[index + len]))
		len++;
	long ret = std::atol(&data[index]);
	index += len;

	return ret;
}

bool json_parser::parse_bool()
{
	if (strncmp(&data[index], "true", 4) == 0)
	{
		index += 4;
		return true;
	}
	index += 5;
	return false;
}

std::vector<json_value> json_parser::parse_array()
{
	std::vector<json_value> ret;

	while (true)
	{
		json_value value = parse();
		ret.emplace(ret.end(), value.value);
		bool brk = false;
		while (true)
		{
			if (data[index] == ',')
			{
				index++;
				break;
			}
			if (data[index] == ']')
			{
				brk = true;
				index++;
				//std::println("Found array end");
				break;
			}
			index++;
		}
		if (brk == true)
			break;
	}
	return ret;
}
