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

void chunk::generate(std::vector<position_int> &trees,chunk_generator &chunk_gen ,int *spawn_y)
{
	int8_t *out_data = chunk_gen.generate(x, z, spawn_y);
	for (int i = 0; i < sections.size(); i++)
	{
		int index = i * 4096;
		std::memcpy(sections[i].blocks.data(), &out_data[index], 4096);
		sections[i].non_air_blocks = 4096;
	}
}


chunk & world::get_chunk(int x, int z)
{
	auto ret = chunks.find(std::make_pair(x, z));
	if (ret == chunks.end())
	{
		auto &chunk = chunks.emplace(std::piecewise_construct, std::forward_as_tuple(x, z), std::forward_as_tuple(x, z)).first->second;
		chunk.generate(trees_to_build, chunkgen);
		return chunk;
	}
	return ret->second;
}

chunk & world::get_chunk(int x, int z, int *spawn_y)
{
	auto ret = chunks.find(std::make_pair(x, z));
	if (ret == chunks.end())
	{
		auto &chunk = chunks.emplace(std::piecewise_construct, std::forward_as_tuple(x, z), std::forward_as_tuple(x, z)).first->second;
		chunk.generate(trees_to_build, chunkgen, spawn_y);
		return chunk;
	}
	return ret->second;
}

void world::generate_seeds()
{
	chunkgen.init();
}


std::expected<bool, chunk_error> world::set_block(int x, int y, int z, std::uint64_t b)
{
	chunk &c = get_chunk(floor((float)x/16.0f), floor((float)z/16.0f));
	auto ret = c.set_block(rem_euclid(x, 16), y, rem_euclid(z, 16), b);
	if (!ret)
	{
		log("Block placement failed", LOG_LEVEL::ERROR);
		return std::unexpected(ret.error());
	}
	return true;
}

void world::set_blocks(std::map<std::string, block> b)
{
	blocks_ = b;
}

void world::build_trees()
{
	using clock = std::chrono::system_clock;
	using ms = std::chrono::duration<double, std::milli>;

	const auto before = clock::now();
	std::uint64_t log_id = get_block("minecraft:oak_log", {});
	std::uint64_t leaves_id = get_block("minecraft:oak_leaves", {});
	for (auto &pos: trees_to_build)
	{
		set_block(pos.x, pos.y + 1, pos.z, log_id);
		set_block(pos.x, pos.y + 2, pos.z, log_id);
		for (int y = 3; y < 5; y++)
		{
			for(int z = -3; z <= 3; z++)
			{
				for(int x = -3; x <= 3; x++)
				{
					if (x == 0 && z == 0)
						set_block(pos.x + x, pos.y + y, pos.z + z, log_id);
					else
						set_block(pos.x + x, pos.y + y, pos.z + z, leaves_id);
				}
			}
		}
		for(int z = -2; z <= 2; z++)
		{
			for(int x = -2; x <= 2; x++)
			{
				if (x == 0 && z == 0)
					set_block(pos.x + x, pos.y + 5, pos.z + z, log_id);
				else
					set_block(pos.x + x, pos.y + 5, pos.z + z, leaves_id);
			}
		}
		set_block(pos.x + 1, pos.y + 6, pos.z, leaves_id);
		set_block(pos.x - 1, pos.y + 6, pos.z, leaves_id);
		set_block(pos.x, pos.y + 6, pos.z, leaves_id);
		set_block(pos.x, pos.y + 6, pos.z + 1, leaves_id);
		set_block(pos.x, pos.y + 6, pos.z - 1, leaves_id);
	}
	trees_to_build.clear();
	const ms duration = clock::now() - before;
	log(std::format("placing trees took {}", duration), LOG_LEVEL::NORMAL);
}

json_value world::get_block_properties(std::string block)
{
	auto b = blocks_.find(block);

	if (b == blocks_.end())
		return json_value(false);

	auto &bl = b->second;

	return bl.propierties.get<json_object>()["properties"];
}

std::uint64_t world::get_block(std::string block, std::map<std::string, json_value> properties)
{
	auto b = blocks_.find(block);

	if (b == blocks_.end())
		return -1;

	auto &bl = b->second;

	json_value ret = bl.propierties.get<json_object>()["states"];
	std::uint64_t def = 0;

	if (properties.empty())
	{
		return bl.actual_id;
	}

    for (auto &state: ret.get<json_array>())
    {
        bool contains_everything = true;
        for (auto &[prop, value]: properties)
        {
            if (state.get<json_object>()["properties"].get<json_object>().contains(prop))
            {
                if (state.get<json_object>()["properties"].get<json_object>()[prop].get_type() == value.get_type())
                {
                    if (value.get_type() == TYPE_JSON::STRING)
                    {
                        if (state.get<json_object>()["properties"].get<json_object>()[prop].get<std::string>() != value.get<std::string>())
                        {
                            contains_everything = false;
                            break;
                        }
                    }
                    else if (value.type == TYPE_JSON::BOOL)
                    {
                        if (state.get<json_object>()["properties"].get<json_object>()[prop].get<bool>() != value.get<bool>())
                        {
                            contains_everything = false;
                            break;
                        }
                    }
                    else if (value.type == TYPE_JSON::NUMBER)
                    {
                        if (state.get<json_object>()["properties"].get<json_object>()[prop].get<long>() != value.get<long>())
                        {
                            contains_everything = false;
                            break;
                        }
                    }
                }
                else
                {
                    contains_everything = false;
                    break;
                }
            }
            else
            {
                contains_everything = false;
                break;
            }
        }

        if (contains_everything)
        {
            return state.get<json_object>()["id"].get<long>();
            break;
        }
    }
	return def;
}

std::uint64_t world::get_block(int x, int y, int z)
{
	chunk &c = get_chunk(floor((float)x/16.0f), floor((float)z/16.0f));
	return c.get_block_id(rem_euclid(x, 16), y, rem_euclid(z, 16));
}

bool world::is_id_block(std::uint64_t id, std::vector<std::string> blocks)
{

	for (auto &block: blocks)
	{
		auto b = blocks_.find(block);

		if (b == blocks_.end())
			return -1;

		auto &bl = b->second;
		json_value ret = bl.propierties.get<json_object>()["states"];
		for (auto &state: ret.get<json_array>())
		{
			if (state.get<json_object>()["id"].get<long>() == id)
				return true;
		}
	}
	return false;
}
