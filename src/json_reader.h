#pragma once
#include <print>
#include <map>
#include <fstream>
#include <variant>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>

enum class TYPE_JSON
{
	BOOL,
	NUMBER,
	STRING,
	ARRAY,
	OBJECT
};



struct json_value;
using json_object = std::map<std::string, json_value>;
using json_array = std::vector<json_value>;
struct json_value
{
	json_value()
	{}
	json_value(std::variant<bool, long, std::string, std::vector<json_value>, std::map<std::string, json_value>> v)
		:value(std::move(v))
	{
		if (std::holds_alternative<bool>(value))
			type = TYPE_JSON::BOOL;
		if (std::holds_alternative<long>(value))
			type = TYPE_JSON::NUMBER;
		if (std::holds_alternative<std::string>(value))
			type = TYPE_JSON::STRING;
		if (std::holds_alternative<std::vector<json_value>>(value))
			type = TYPE_JSON::ARRAY;
		if (std::holds_alternative<std::map<std::string, json_value>>(value))
			type = TYPE_JSON::OBJECT;
	}
	std::variant<bool, long, std::string, std::vector<json_value>, std::map<std::string, json_value>> value;
	TYPE_JSON type;

	TYPE_JSON get_type()
	{
		if (std::holds_alternative<bool>(value))
			return TYPE_JSON::BOOL;
		if (std::holds_alternative<long>(value))
			return TYPE_JSON::NUMBER;
		if (std::holds_alternative<std::string>(value))
			return TYPE_JSON::STRING;
		if (std::holds_alternative<std::vector<json_value>>(value))
			return TYPE_JSON::ARRAY;
		if (std::holds_alternative<std::map<std::string, json_value>>(value))
			return TYPE_JSON::OBJECT;
		return TYPE_JSON::BOOL;
	}

	template <typename T>
	T get()
	{
		return std::get<T>(value);
	}
};


class json_parser
{
	public:
		json_parser(char *d)
			:data(d)
		{
			index = 0;
		}

		json_value parse();

	private:
		std::string parse_string();
		std::map<std::string, json_value> parse_object();
		long parse_number();
		bool parse_bool();
		std::vector<json_value> parse_array();
		char *data;
		int index;
};
