#pragma once
#include <expected>
#include <print>
#include <vector>
#include <map>
#include <cmath>
#include <random>
#include "user.h"
#include "PerlinNoise.hpp"
#include "log.h"
#include "blocks.h"

int rem_euclid(int a, int b);

struct dot
{
	double start_noise_val; double end_noise_val;
	int start_value; int end_value;

	int get_height(double noise_value);
};

struct spline
{
	std::vector<dot> dots;

	int get_value(double noise_value);
};

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
	chunk(int x, int z)
		:x(x), z(z)
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
	int x, z;
	std::expected<bool, chunk_error> set_block(int x, int y, int z, int id);
	void generate(std::vector<position_int> &trees, siv::PerlinNoise &continentality_noise, siv::PerlinNoise &main_noise, siv::PerlinNoise &bush_noise, siv::PerlinNoise &tree_noise);
	void generate(std::vector<position_int> &trees, siv::PerlinNoise &continentality_noise, siv::PerlinNoise &main_noise, int &spawn_y);

};

struct world
{
	std::map<std::pair<int, int>, chunk> chunks;
	std::vector<position_int> trees_to_build;
	siv::PerlinNoise::seed_type continentality_seed;
	siv::PerlinNoise::seed_type erosion_seed;
	siv::PerlinNoise::seed_type bush_seed;
	siv::PerlinNoise::seed_type tree_seed;
	siv::PerlinNoise continentality_noise;
	siv::PerlinNoise main_noise;
	siv::PerlinNoise bush_noise;
	siv::PerlinNoise tree_noise;
	chunk &get_chunk(int x, int z);
	chunk &get_chunk(int x, int z, int &spawn_y);
	void generate_seeds();
	std::expected<bool, chunk_error> set_block(int x, int y, int z, int id);
	void build_trees(long log_id, long leaves_id);
};

