#include "chunk.h"
#include <print>
#include <utility>

int rem_euclid(int a, int b)
{
	int ret = a %b;
	if (ret < 0)
	{
		ret += b;
	}
	return ret;
}

std::expected<bool, chunk_error> chunk::set_block(int place_x, int place_y, int place_z, int id)
{
	if (place_x >= 16 || place_y >= 320 || place_z >= 16)
		return std::unexpected(chunk_error::NON_EXISTING_POSITION);
	int section_index = (place_y + 64)/16;
	section &sec = sections[section_index];
	char palette_index = -1;
	for (int i = 0; i < sec.palette.size(); i++)
	{
		if (sec.palette[i] == id)
		{
			if (sec.blocks.size() == 1)
			{
				if (i != sec.blocks[0])
					sec.blocks.resize(4096);
			}
			palette_index = i;
			break;
		}
	}
	if (palette_index == -1)
	{
		if (sec.blocks.empty() || sec.blocks.size() == 1)
			sec.blocks.resize(4096);
		sec.palette.push_back(id);
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

void chunk::generate(std::vector<std::pair<int, int>> &trees)
{
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> dist(1,20);

	int tree_x = dist(rng);
	int tree_z = dist(rng);
	if (tree_x < 16 && tree_z < 16)
		trees.push_back(std::make_pair(tree_x + (x * 16), tree_z + (y * 16)));
	for (int z = 0; z < 16; z++)
	{
		for (int x_ = 0; x_ < 16; x_++)
		{
			for (int y_ = -64; y_ < 64; y_++)
			{
				auto ret = set_block(x_, y_, z, 9);
				if (!ret)
					std::println("Set block failed!");
			}
		}
	}
}



chunk & world::get_chunk(int x, int z)
{
	auto ret = chunks.find(std::make_pair(x, z));
	if (ret == chunks.end())
	{
		auto &chunk = chunks.emplace(std::piecewise_construct, std::forward_as_tuple(x, z), std::forward_as_tuple(x, z)).first->second;
		chunk.generate(trees_to_build);
		return chunk;
	}
	return ret->second;
}

std::expected<bool, chunk_error> world::set_block(int x, int y, int z, long id)
{
	chunk &c = get_chunk(floor((float)x/16.0f), floor((float)z/16.0f));
	auto ret = c.set_block(rem_euclid(x, 16), y, rem_euclid(z, 16), id);
	if (!ret)
	{
		std::println("Block placement failed");
		return std::unexpected(ret.error());
	}
	return true;
}

void world::build_trees(long log_id, long leaves_id)
{
	for (auto &pos: trees_to_build)
	{
		set_block(pos.first, 64, pos.second, log_id);
		set_block(pos.first, 65, pos.second, log_id);
		for (int y = 66; y < 68; y++)
		{
			for(int z = -3; z <= 3; z++)
			{
				for(int x = -3; x <= 3; x++)
				{
					if (x == 0 && z == 0)
						set_block(pos.first + x, y, pos.second + z, log_id);
					else
						set_block(pos.first + x, y, pos.second + z, leaves_id);
				}
			}
		}
		for(int z = -2; z <= 2; z++)
		{
			for(int x = -2; x <= 2; x++)
			{
				if (x == 0 && z == 0)
					set_block(pos.first + x, 68, pos.second + z, log_id);
				else
					set_block(pos.first + x, 68, pos.second + z, leaves_id);
			}
		}
		set_block(pos.first + 1, 69, pos.second, leaves_id);
		set_block(pos.first - 1, 69, pos.second, leaves_id);
		set_block(pos.first, 69, pos.second, leaves_id);
		set_block(pos.first, 69, pos.second + 1, leaves_id);
		set_block(pos.first, 69, pos.second - 1, leaves_id);
	}
	trees_to_build.clear();
}
