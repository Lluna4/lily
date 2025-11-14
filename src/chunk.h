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

static std::map<std::string, block> blocks_;

static std::uint64_t get_block(std::string block, std::map<std::string, json_value> properties)
{
	auto b = blocks_.find(block);

	if (b == blocks_.end())
		return -1;

	auto &bl = b->second;
	if (properties.empty())
		return bl.actual_id;

	json_value ret = bl.propierties.get<json_object>()["states"];
	std::uint64_t def = 0;

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
			sec.blocks.push_back(0);
			sec.palette.push_back(get_block("minecraft:air", {}));
			sec.non_air_blocks = 0;
			sections.push_back(sec);
		}
	}
	std::vector<section> sections;
	int x, z;
	std::uint64_t get_block_id(int place_x, int place_y, int place_z);
	std::expected<bool, chunk_error> set_block(int place_x, int place_y, int place_z, std::uint64_t b);
	void generate(std::vector<position_int> &trees, siv::PerlinNoise &continentality_noise, siv::PerlinNoise &main_noise, siv::PerlinNoise &bush_noise, siv::PerlinNoise &tree_noise, int *spawn_y = nullptr);
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

	void set_blocks(std::map<std::string, block> blocks);
	chunk &get_chunk(int x, int z);
	chunk &get_chunk(int x, int z, int *spawn_y);
	void generate_seeds();
	std::expected<bool, chunk_error> set_block(int x, int y, int z, std::uint64_t b);
	void build_trees();
	json_value get_block_properties(std::string block);
	std::uint64_t get_block(std::string block, std::map<std::string, json_value> properties);
	std::uint64_t get_block(int x, int y, int z);
	bool is_id_block(std::uint64_t id, std::vector<std::string>);
};

