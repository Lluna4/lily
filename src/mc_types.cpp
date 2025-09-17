#include "mc_types.h"

void minecraft::uuid::generate(std::string name)
{
	unsigned char *buff = (unsigned char *)calloc(17, sizeof(unsigned char));
	EVP_MD_CTX *mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
	EVP_DigestUpdate(mdctx, name.c_str(), name.length());
	EVP_DigestFinal_ex(mdctx, buff, nullptr);
	EVP_MD_CTX_destroy(mdctx);
	std::string buf((char *)buff);
}

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

void minecraft::read_string(char *dat, minecraft::string &out)
{
	varint size = read_varint(dat);

	dat += size.size;
	out.data.write(dat, size.num);
	out.size = (size.size + size.num);
}
