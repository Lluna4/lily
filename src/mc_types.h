#pragma once
#include <cstddef>
#include <openssl/evp.h>
#include <string>
#include "networking/buffer.h"

namespace minecraft
{
	struct varint
	{
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


	varint read_varint(const char* addr);
	size_t write_varint(char *dest, unsigned long val);
	void read_string(char *dat, string &out);
}