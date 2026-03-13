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

enum class TREE_TYPE
{
	OAK,
	TAIGA,
	CACTUS
};

struct tree
{
	tree(position_int p, TREE_TYPE t)
	:pos(p), type(t)
	{}
	position_int pos;
	TREE_TYPE type;
};

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
	std::vector<std::uint64_t> biome_palette;
	std::vector<int8_t> biome;
	short non_air_blocks;
};

enum class chunk_error
{
	NON_EXISTING_POSITION
};

struct chunk
{
	chunk(int x, int z, std::vector<std::string> &biomes)
		:x(x), z(z)
	{
		for (int i = 0; i < 24; i++)
		{
			section sec;
			sec.blocks.push_back(0);
			sec.blocks.resize(4096);
			for (int xx = 1; xx < 4096; xx++)
				sec.blocks[xx] = sec.blocks[0];
			sec.palette.push_back(get_block("minecraft:air", {}));
			sec.palette.push_back(get_block("minecraft:stone", {}));
			sec.palette.push_back(get_block("minecraft:water", {}));
			sec.palette.push_back(get_block("minecraft:grass_block", {}));
			sec.palette.push_back(get_block("minecraft:tall_grass", {}));
			sec.palette.push_back(get_block("minecraft:tall_grass", {{"half", json_value("upper")}}));
			sec.palette.push_back(get_block("minecraft:short_grass", {}));
			sec.palette.push_back(get_block("minecraft:oak_log", {}));
			sec.palette.push_back(get_block("minecraft:oak_leaves", {}));
			sec.palette.push_back(get_block("minecraft:dirt", {}));
			sec.palette.push_back(get_block("minecraft:sand", {}));
			sec.palette.push_back(get_block("minecraft:grass_block", {{"snowy", json_value("true")}}));
			sec.palette.push_back(get_block("minecraft:snow", {}));
			sec.palette.push_back(get_block("minecraft:ice", {}));
			sec.palette.push_back(get_block("minecraft:dead_bush", {}));
			sec.non_air_blocks = 4096;
			sec.biome.resize(128);
			sec.biome_palette.push_back(std::distance(biomes.begin(), std::find(biomes.begin(), biomes.end(), "plains")));
			sec.biome_palette.push_back(std::distance(biomes.begin(), std::find(biomes.begin(), biomes.end(), "snowy_plains")));
			sec.biome_palette.push_back(std::distance(biomes.begin(), std::find(biomes.begin(), biomes.end(), "desert")));
			sec.biome_palette.push_back(std::distance(biomes.begin(), std::find(biomes.begin(), biomes.end(), "taiga")));
			sections.push_back(sec);
		}
	}
	std::vector<section> sections;
	int x, z;
	std::uint64_t get_block_id(int place_x, int place_y, int place_z);
	std::expected<bool, chunk_error> set_block(int place_x, int place_y, int place_z, std::uint64_t b);
	void set_block_direct(int place_x, int place_y, int place_z, std::uint64_t b);
	void generate(std::vector<tree> &trees, siv::PerlinNoise &continentality_noise, siv::PerlinNoise &main_noise, siv::PerlinNoise &bush_noise, siv::PerlinNoise &tree_noise, siv::PerlinNoise &temperature, siv::PerlinNoise &cave_noise, std::vector<std::string> &biomes, int *spawn_y = nullptr);
};

struct world
{
	std::map<std::pair<long, long>, chunk> chunks;
	std::vector<tree> trees_to_build;
	std::vector<std::string> biomes;
	siv::PerlinNoise::seed_type continentality_seed;
	siv::PerlinNoise::seed_type erosion_seed;
	siv::PerlinNoise::seed_type bush_seed;
	siv::PerlinNoise::seed_type tree_seed;
	siv::PerlinNoise::seed_type temperature_seed;
	siv::PerlinNoise::seed_type cave_seed;
	siv::PerlinNoise continentality_noise;
	siv::PerlinNoise main_noise;
	siv::PerlinNoise bush_noise;
	siv::PerlinNoise tree_noise;
	siv::PerlinNoise temperature;
	siv::PerlinNoise cave_noise;

	void set_blocks(std::map<std::string, block> blocks);
	chunk &get_chunk(long x, long z);
	chunk &get_chunk(long x, long z, int *spawn_y);
	void generate_seeds();
	std::expected<bool, chunk_error> set_block(long x, long y, long z, std::uint64_t b);
	void set_block_direct(long x, long y, long z, std::uint64_t b);
	void build_trees();
	json_value get_block_properties(std::string block);
	std::uint64_t get_block(std::string block, std::map<std::string, json_value> properties);
	std::uint64_t get_block(long x, long y, long z);
	bool is_id_block(std::uint64_t id, std::vector<std::string>);
};

