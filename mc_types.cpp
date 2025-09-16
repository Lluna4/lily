#include "mc_types.h"

minecraft::varint minecraft::read_varint(const char* addr)
{
	unsigned long result = 0;
	int shift = 0;
	std::size_t count = 0;
	minecraft::varint ret = {0};

	while (true)
	{
		unsigned char byte = *reinterpret_cast<const unsigned char*>(addr);
		addr++;
		count++;

		result |= (byte & 0x7f) << shift;
		shift += 7;

		if (!(byte & 0x80)) break;
	}

	ret.num = result;
	ret.size = count;

	return ret;
}

size_t minecraft::write_varint(char *dest, unsigned long val)
{
	size_t count = 0;
	do {
		unsigned char byte = val & 0x7f;
		val >>= 7;

		if (val != 0)
			byte |= 0x80;
		dest[count] = byte;
		count++;
	} while (val != 0);
	return count;
}