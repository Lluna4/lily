#pragma once
#include <cstddef>
#include <memory>
#include <meta>
#include <zlib.h>
#include "mc_types.h"
#include <bit>
#include <bitset>
#include "chunk.h"
#include <type_traits>
#ifdef __FreeBSD__
#include <sys/endian.h>
#endif
#ifdef __APPLE__
#include <libkern/OSByteOrder.h>


#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)
#endif

struct pkt_header
{
	minecraft::varint size;
	minecraft::varint id;
};

template <typename T>
void deserialize(T &s, char *data)
{
	if (std::meta::is_class_type(^^T))
	{
		constexpr auto static items = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));
		int offset = 0;
		template for (constexpr auto item: items)
		{
			if constexpr (std::meta::type_of(item) == ^^minecraft::varint)
			{
				s.[:item:] = minecraft::read_varint(&data[offset]);
				offset += s.[:item:].size;
			}
			else if constexpr (std::meta::type_of(item) == ^^minecraft::string)
			{
				minecraft::read_string(&data[offset], s.[:item:]);
				offset += s.[:item:].size;
			}
			else if constexpr (std::meta::type_of(item) == ^^double)
			{
				std::uint64_t num_as_uint64;
				double num;
			
				memcpy(&num_as_uint64, &data[offset], sizeof(std::uint64_t));
				num_as_uint64 = be64toh(num_as_uint64);
				memcpy(&num, &num_as_uint64, sizeof(double));
				s.[:item:] = num;

				offset += sizeof(double);
			}
			else if constexpr (std::meta::type_of(item) == ^^float)
			{
				std::uint32_t num_as_uint32;
				float num;
			
				memcpy(&num_as_uint32, &data[offset], sizeof(std::uint32_t));
				num_as_uint32 = be32toh(num_as_uint32);
				memcpy(&num, &num_as_uint32, sizeof(float));
				s.[:item:] = num;

				offset += sizeof(float);
			}
			else
			{
				memcpy(&s.[:item:], &data[offset], sizeof(s.[:item:]));
				s.[:item:] = std::byteswap(s.[:item:]);				
				offset += sizeof(s.[:item:]);
			}
		}
	}
}

template <typename T>
int serialize(T &s, char *data)
{
	int offset = 0;
	if (std::meta::is_class_type(^^T))
	{
		constexpr auto static items = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));

		template for (constexpr auto item: items)
		{
			if constexpr (std::is_same_v<std::remove_cvref_t<decltype(s.[:item:])>, minecraft::varint>)
			{
				offset += minecraft::write_varint(&data[offset], s.[:item:].num);
			}
			else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(s.[:item:])>, std::string>)
			{
				offset += minecraft::write_varint(&data[offset], s.[:item:].size());
				memcpy(&data[offset], s.[:item:].c_str(), s.[:item:].size());
				offset += s.[:item:].size();
			}
			else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(s.[:item:])>, minecraft::uuid>)
			{
				uint64_t msb = 0;
				uint64_t lsb = 0;
				memcpy(&msb, s.[:item:].data.c_str(), sizeof(uint64_t));
				memcpy(&lsb, &s.[:item:].data.c_str()[7], sizeof(uint64_t));
				memcpy(&data[offset], &lsb, sizeof(uint64_t));
				offset += sizeof(uint64_t);
				memcpy(&data[offset], &msb, sizeof(uint64_t));
				offset += sizeof(uint64_t);

			}
			else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(s.[:item:])>, double>)
			{
				uint64_t conv = htobe64((*(uint64_t *)&s.[:item:]));
				memcpy(&data[offset], &conv, sizeof(s.[:item:]));
				offset += sizeof(s.[:item:]);
			}
			else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(s.[:item:])>, float>)
			{
				uint32_t conv = htobe64((*(uint32_t *)&s.[:item:]));
				memcpy(&data[offset], &conv, sizeof(s.[:item:]));
				offset += sizeof(s.[:item:]);
			}
			else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(s.[:item:])>, chunk>)
			{

				std::unique_ptr<char []> tmp = std::make_unique<char []>(s.[:item:].encoded_data_size);
				size_t real_size = 0;

				for (auto &sec: s.[:item:].sections)
				{
					short non_air = htobe16(*(uint16_t*)&sec.non_air_blocks);
					memcpy(&tmp.get()[real_size], &non_air, sizeof(short));
					real_size += sizeof(short);
					if (sec.blocks.size() == 1)
					{
						tmp.get()[real_size] = 0;
						real_size++;
						real_size += minecraft::write_varint(&tmp.get()[real_size], sec.palette[sec.blocks[0]]);
					}
					else
					{
						tmp.get()[real_size] = 8;
						real_size++;
						real_size += minecraft::write_varint(&tmp.get()[real_size], sec.palette.size());
						for (auto &p: sec.palette)
						{
							real_size += minecraft::write_varint(&tmp.get()[real_size], p);
						}
						for (int i = 0; i < 512; i++)
						{
							int64_t tmp_l = 0;
							memcpy(&tmp_l, &sec.blocks[i * 8], sizeof(int64_t));
							tmp_l = htobe64((*(uint64_t *)&tmp_l));
							memcpy(&tmp.get()[real_size], &tmp_l, sizeof(int64_t));
							real_size += sizeof(int64_t);
						}
					}
					tmp.get()[real_size] = 1;
					real_size++;
					
					real_size += minecraft::write_varint(&tmp.get()[real_size], sec.biome_palette.size());
					for (auto &p: sec.biome_palette)
					{
						real_size += minecraft::write_varint(&tmp.get()[real_size], p);
					}
					std::bitset<64> l;
					for (int i = 0; i < 64; i++)
					{
						l[i] = sec.biome[i];
					}
					uint64_t tmp_l = l.to_ulong();
					tmp_l = htobe64((*(uint64_t *)&tmp_l));
					memcpy(&tmp.get()[real_size], &tmp_l, sizeof(int64_t));
					real_size += sizeof(int64_t);
				}
				
				offset += minecraft::write_varint(&data[offset], real_size);
				memcpy(&data[offset], tmp.get(), real_size);
				
				
				offset += real_size;
			}
			else
			{
				s.[:item:] = std::byteswap(s.[:item:]);
				memcpy(&data[offset], &s.[:item:], sizeof(s.[:item:]));
				offset += sizeof(s.[:item:]);
			}
		}
	}
	return offset;
}

template <typename T>
int size_of(T &s)
{
	int size = 0;
	if (std::meta::is_class_type(^^T))
	{
		constexpr auto static items = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));

		template for (constexpr auto item: items)
		{
			if constexpr (std::meta::type_of(item) == ^^minecraft::varint)
			{
				char dummy[10];
				size += minecraft::write_varint(dummy, s.[:item:].num);
			}
			else if constexpr (std::meta::type_of(item) == ^^std::string)
			{
				char dummy[10];
				size += minecraft::write_varint(dummy, s.[:item:].size());
				size += s.[:item:].size();
			}
			else if constexpr (std::meta::type_of(item) == ^^minecraft::uuid)
			{
				size += 16;
			}
			else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(s.[:item:])>, chunk>)
			{
				s.[:item:].encoded_data_size = 0;
				char dummy[10];

				for (auto &sec: s.[:item:].sections)
				{
					short non_air = htobe16(*(uint16_t*)&sec.non_air_blocks);
					s.[:item:].encoded_data_size += sizeof(short);
					if (sec.blocks.size() == 1)
					{
						s.[:item:].encoded_data_size++;
						s.[:item:].encoded_data_size += minecraft::write_varint(dummy, sec.palette[sec.blocks[0]]);
					}
					else
					{
						s.[:item:].encoded_data_size++;

						s.[:item:].encoded_data_size += minecraft::write_varint(dummy, sec.palette.size());
						for (auto &p: sec.palette)
						{
							s.[:item:].encoded_data_size += minecraft::write_varint(dummy, p);
						}
						for (int i = 0; i < 512; i++)
						{
							int64_t tmp = 0;
							memcpy(&tmp, &sec.blocks[i * 8], sizeof(int64_t));
							tmp = htobe64((*(uint64_t *)&tmp));
							s.[:item:].encoded_data_size += sizeof(int64_t);
						}
					}
					s.[:item:].encoded_data_size++;
					s.[:item:].encoded_data_size += minecraft::write_varint(dummy, sec.biome_palette.size());
					for (auto &p: sec.biome_palette)
					{
						s.[:item:].encoded_data_size += minecraft::write_varint(dummy, p);
					}
					std::bitset<64> l;
					for (int i = 0; i < 64; i++)
					{
						l[i] = sec.biome[i];
					}
					uint64_t tmp = l.to_ulong();
					tmp = htobe64((*(uint64_t *)&tmp));
					s.[:item:].encoded_data_size += sizeof(uint64_t);
				}
				s.[:item:].encoded_data_size += minecraft::write_varint(dummy, s.[:item:].encoded_data_size);
				size += s.[:item:].encoded_data_size;
			}
			else
				size += sizeof(s.[:item:]);
		}
	}
	return size;
}

static packet generate_packet_compressed(int fd, size_t uncompressed_size, size_t size, char *data)
{
	char dummy[10];

	int uncompress_size_len = minecraft::write_varint(dummy, uncompressed_size);
	int packet_len_len = minecraft::write_varint(dummy, uncompress_size_len + size);

	packet pkt;
	pkt.size = packet_len_len + uncompress_size_len + size;
	pkt.data = std::make_unique<char []>(pkt.size);
	pkt.fd = fd;
	pkt_header head = {.size = minecraft::varint(uncompress_size_len + size), .id = minecraft::varint(uncompressed_size)};
	int offset = serialize(head, pkt.data.get());
	memcpy(pkt.data.get() + offset, data, size);

	return std::move(pkt);

}

static packet generate_packet(int fd, int id, size_t size, char *data)
{
	char dummy[10];
	int id_len = minecraft::write_varint(dummy, id);

	int payload_len = size + id_len;
	int packet_len_varint_size = minecraft::write_varint(dummy, payload_len);

	packet pkt;
	pkt.size = packet_len_varint_size + payload_len;
	pkt.data = std::make_unique<char []>(packet_len_varint_size + payload_len);
	pkt.id = id;
	pkt.fd = fd;
	pkt_header head = {.size = minecraft::varint(payload_len), .id = minecraft::varint(pkt.id)};
	int offset = serialize(head, pkt.data.get());
	memcpy(pkt.data.get() + offset, data, size);

	return std::move(pkt);

}

template <typename T>
void send_packet_compressed(int fd, int id, T &resp, server &sv)
{
	int size = size_of(resp);
	char dummy[10];
	int id_len = minecraft::write_varint(dummy, id);
	std::unique_ptr<char []> dat = std::make_unique<char []>(size + id_len);
	minecraft::write_varint(dat.get(), id);
	serialize(resp, &dat.get()[id_len]);
	unsigned long dest_size = compressBound(size + id_len);
	std::unique_ptr<char []> dat_compressed = std::make_unique<char []>(dest_size);

	std::println("{}", compress((unsigned char *)dat_compressed.get(), &dest_size, (unsigned char *)dat.get(), size + id_len));
	packet pkt = generate_packet_compressed(fd, size + id_len, dest_size, dat_compressed.get());
	pkt.id = id;
	sv.send_packet(std::move(pkt), fd, id);
}

template <typename T>
void send_packet(int fd, int id, T &resp, server &sv)
{
	int size = size_of(resp);
	std::unique_ptr<char []> dat = std::make_unique<char []>(size);
	
	serialize(resp, dat.get());
	packet pkt = generate_packet(fd, id, size, dat.get());

	sv.send_packet(std::move(pkt), fd, id);
}