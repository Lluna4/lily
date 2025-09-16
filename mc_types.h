#pragma once
#include <cstddef>

namespace minecraft
{
	struct varint
	{
		unsigned long num;
		size_t size;
	};

	varint read_varint(const char* addr);
	size_t write_varint(char *dest, unsigned long val);
}