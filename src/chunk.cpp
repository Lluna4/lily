#include "chunk.h"
#include "json_reader.h"
#include "log.h"
#include <cstdint>

int rem_euclid(int a, int b)
{
	int ret = a %b;
	if (ret < 0)
	{
		ret += b;
	}
	return ret;
}

std::uint64_t chunk::get_block_id(int place_x, int place_y, int place_z)
{
	if (place_x >= 16 || place_y >= 320 || place_z >= 16)
		return -1;
	int section_index = (place_y + 64)/16;
	section &sec = sections[section_index];
	std::int8_t index = -1;
	if (sec.blocks.size() == 1)
		index = sec.blocks[0];
	else
		index = sec.blocks[(rem_euclid(place_y, 16) * 256) + (place_z * 16) + place_x];
	return sec.palette[index];
}

std::expected<bool, chunk_error> chunk::set_block(int place_x, int place_y, int place_z, std::uint64_t b)
{
	if (place_x >= 16 || place_y >= 320 || place_z >= 16)
		return std::unexpected(chunk_error::NON_EXISTING_POSITION);
	int section_index = (place_y + 64)/16;
	section &sec = sections[section_index];
	char palette_index = -1;
	for (int i = 0; i < sec.palette.size(); i++)
	{
		if (sec.palette[i] == b)
		{
			if (sec.blocks.size() == 1)
			{
				if (i != sec.blocks[0])
				{
					sec.blocks.resize(4096);
					for (int i = 1; i < 4096; i++)
						sec.blocks[i] = sec.blocks[0];
				}
			}
			palette_index = i;
			break;
		}
	}
	if (palette_index == -1)
	{
		if (sec.blocks.empty() || sec.blocks.size() == 1)
		{
			sec.blocks.resize(4096);
			for (int i = 1; i < 4096; i++)
				sec.blocks[i] = sec.blocks[0];
		}
		sec.palette.push_back(b);
		palette_index = sec.palette.size() - 1;
	}

	sec.blocks[(rem_euclid(place_y, 16) * 256) + (place_z * 16) + place_x] = palette_index;
	sec.non_air_blocks++;
	if (sec.non_air_blocks > 4096)
		sec.non_air_blocks = 4096;
	if (sec.non_air_blocks == 4096)
	{
		bool equal = false;
		long equal_id = 0;
		for (long id_ = 0; id_ < sec.palette.size(); id_++)
		{
			bool eq = true;
			for (auto &block: sec.blocks)
			{
				if (block != id_)
				{
					eq = false;
					break;
				}
			}
			if (eq == true)
			{
				equal_id = id_;
				equal = true;
				break;
			}
		}
		if (equal == true)
		{
			sec.blocks.clear();
			sec.blocks.shrink_to_fit();
			sec.blocks.push_back(equal_id);
		}
	}
	return true;
}

/*void chunk::generate(std::vector<position_int> &trees,chunk_generator &chunk_gen ,int *spawn_y)
{
	using clock = std::chrono::system_clock;
	using ms = std::chrono::duration<double, std::milli>;
	
	int8_t *out_data = chunk_gen.generate(x, z, spawn_y);
	const auto before = clock::now();
	size_t size = 4096 * 24;
	for (int i = 0; i < sections.size(); i++)
	{
		int index = i * 4096;
		std::memcpy(sections[i].blocks.data(), &out_data[index], 4096);
		sections[i].non_air_blocks = 4096;
	}
	memset(out_data, 0, size);
	const ms duration = clock::now() - before;
	log(std::format("Chunk update took {}", duration), LOG_LEVEL::NORMAL);
}*/

