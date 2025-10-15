#pragma once
#include <expected>
#include <print>
#include <vector>
#include <map>
#include <cmath>

int rem_euclid(int a, int b);

struct coordinates
{
	coordinates(int x_, int y_)
		:x(x_), y(y_)
	{}
	int x, y;

	bool operator<(const coordinates &c) const
	{
		if (x + y < c.x + c.y)
			return true;
		return false;
	}
};

struct section
{
	std::vector<u_int64_t> palette;
	std::vector<int8_t> blocks;
	short non_air_blocks;
};

enum class chunk_error
{
	NON_EXISTING_POSITION
};

struct chunk
{
	chunk(int x, int y)
		:x(x), y(y)
	{
		for (int i = 0; i < 24; i++)
		{
			section sec;
			sec.blocks.push_back(0);
			sec.palette.push_back(0);
			sec.non_air_blocks = 0;
			sections.push_back(sec);
		}
	}
	std::vector<section> sections;
	int x, y;
	std::expected<bool, chunk_error> set_block(int x, int y, int z, int id);
	void generate();
};

struct world
{
	std::map<std::pair<int, int>, chunk> chunks;

	chunk &get_chunk(int x, int z);
	std::expected<bool, chunk_error> set_block(int x, int y, int z, long id);
};

