#pragma once
#include <cstddef>
#include <openssl/evp.h>
#include <string>
#include <utility>
#include "networking/buffer.h"
#include "chunk.h"

namespace minecraft
{
	struct varint
	{
		varint()
		{
			num = 0;
			size = 0;
		}
		explicit varint(long n)
			:num(n)
		{
			size = 0;
		}
		unsigned long num;
		size_t size;
	};

	struct uuid
	{
		std::string data;

		void generate(std::string name);
	};

	struct string
	{
		buffer<char> data;
		size_t size;
	};

	struct string_tag
	{
		explicit string_tag(std::string s, std::string t)
			:str(std::move(s)), title(std::move(t))
		{}
		std::string str;
		std::string title;
	};

	struct short_string
	{
		explicit short_string(std::string s)
			:str(std::move(s))
		{}
		std::string str;
	};

	struct nameless_string_tag
	{
		explicit nameless_string_tag(std::string s)
			:str(std::move(s))
		{}
		std::string str;
	};


	varint read_varint(const char* addr);
	size_t write_varint(char *dest, unsigned long val);
	void read_string(char *dat, string &out);
}