#pragma once
#include <meta>
#include "mc_types.h"
#include <bit>

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
			if constexpr (std::meta::type_of(item) == ^^minecraft::varint)
			{
				offset += minecraft::write_varint(&data[offset], s.[:item:].num);
			}
			else if constexpr (std::meta::type_of(item) == ^^std::string)
			{
				offset += minecraft::write_varint(&data[offset], s.[:item:].size());
				memcpy(&data[offset], s.[:item:].c_str(), s.[:item:].size());
				offset += s.[:item:].size();
			}
			else if constexpr (std::meta::type_of(item) == ^^minecraft::uuid)
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
			else
				size += sizeof(s.[:item:]);
		}
	}
	return size;
}

static packet generate_packet(int fd, int id, size_t size, char *data)
{
	char dummy[10];
	int id_len = minecraft::write_varint(dummy, 0x00);

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
void send_packet(int fd, int id, T &resp, server &sv)
{
	int size = size_of(resp);
	std::unique_ptr<char []> dat = std::make_unique<char []>(size);
	
	serialize(resp, dat.get());
	packet pkt = generate_packet(fd, id, size, dat.get());

	sv.send_packet(std::move(pkt), fd, id);
}