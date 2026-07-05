#pragma once
#include <meta>
#include "mc_types.h"


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
			else
			{
				memcpy(&data[offset], &s.[:item:], sizeof(s.[:item:]));
				offset += sizeof(s.[:item:]);
			}
		}
	}
	return offset;
}