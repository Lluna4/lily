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

struct json_value
{
	json_value()
	{
		b = false;
		num = 0;
		type = TYPE_JSON::BOOL;
	}

	json_value(bool bo, long n, std::string s, std::vector<json_value> a, std::map<std::string, json_value> o, TYPE_JSON t)
		:b(bo), num(n), str(s), array(a), object(o), type(t)
	{}
	bool b;
	long num;
	std::string str;
	std::vector<json_value> array;
	std::map<std::string, json_value> object;
	TYPE_JSON type;
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