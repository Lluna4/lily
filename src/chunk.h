#pragma once
#include <cstdint>
#include <expected>
#include <functional>
#include <print>
#include <vector>
#include <map>
#include <cmath>
#include <random>
#include "json_reader.h"
#include "user.h"
#include "PerlinNoise.hpp"
#include "log.h"
#include "blocks.h"

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
	std::vector<std::uint64_t> palette;
	std::vector<int8_t> blocks;
	short non_air_blocks;
};

enum class chunk_error
{
	NON_EXISTING_POSITION
};

struct chunk
{
	chunk(int x, int z)
		:x(x), z(z)
	{
		for (int i = 0; i < 24; i++)
		{
			section sec;
			sec.blocks.resize(4096);
			for (int x = 1; x < 4096; x++)
				sec.blocks[x] = sec.blocks[0];
			sec.palette.push_back(0);
			sec.palette.push_back(9);
			sec.non_air_blocks = 4096;
			sections.push_back(sec);
		}
	}
	std::vector<section> sections;
	int x, z;
	std::uint64_t get_block_id(int place_x, int place_y, int place_z);
	std::expected<bool, chunk_error> set_block(int place_x, int place_y, int place_z, std::uint64_t b);
};


