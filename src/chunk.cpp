#include "chunk.h"
#include "json_reader.h"
#include "log.h"
#include "user.h"
#include <cstdint>
#include <utility>

spline continentalness = {.dots = {{.start_noise_val = -1.0f, .end_noise_val = -0.8f, .start_value = 10, .end_value = 40},
								   {.start_noise_val = -0.8f, .end_noise_val = -0.5f, .start_value = 40, .end_value = 45},
								   {.start_noise_val = -0.5f, .end_noise_val = -0.3f, .start_value = 45, .end_value = 64},
								   {.start_noise_val = -0.3f, .end_noise_val = 0.5f, .start_value = 64, .end_value = 90},
								   {.start_noise_val = 0.5f, .end_noise_val = 0.7f, .start_value = 90, .end_value = 120},
								   {.start_noise_val = 0.7f, .end_noise_val = 0.9f, .start_value = 120, .end_value = 170},
								   {.start_noise_val = 0.9f, .end_noise_val = 1.01f, .start_value = 170, .end_value = 250}}};
							   
int rem_euclid(int a, int b)
{
	int ret = a %b;
	if (ret < 0)
	{
		ret += b;
	}
	return ret;
}

int dot::get_height(double noise_value)
{
	int n = abs(end_value - start_value)/abs(end_noise_val - start_noise_val);

	return n * (noise_value - start_noise_val) + start_value;
}

int spline::get_value(double noise_value)
{
	for (auto &d: dots)
	{
		if (noise_value >= d.start_noise_val && noise_value < d.end_noise_val)
		{
			return d.get_height(noise_value);
		}
	}
	return 0;
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

void chunk::set_block_direct(int place_x, int place_y, int place_z, std::uint64_t b)
{
	if (place_x >= 16 || place_y >= 320 || place_z >= 16)
		return;
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
}

void chunk::generate(std::vector<tree> &trees, siv::PerlinNoise &continentality_noise, siv::PerlinNoise &main_noise, siv::PerlinNoise &bush_noise, siv::PerlinNoise &tree_noise, siv::PerlinNoise &temperature, siv::PerlinNoise &cave_noise, std::vector<std::string> &biomes,int *spawn_y)
{
	using clock = std::chrono::system_clock;
	using ms = std::chrono::duration<double, std::milli>;

	const auto before = clock::now();
	int y_max = 64;
	std::uint64_t dirt = get_block("minecraft:dirt", {});
	std::uint64_t stone = get_block("minecraft:stone", {});
	std::uint64_t grass_id = get_block("minecraft:grass_block", {});
	std::uint64_t tall_grass_lower = get_block("minecraft:tall_grass", {});
	std::uint64_t tall_grass_upper = get_block("minecraft:tall_grass", {{"half", json_value("upper")}});
	std::uint64_t water = get_block("minecraft:water", {});
	std::uint64_t short_grass = get_block("minecraft:short_grass", {});
	std::uint64_t sand = get_block("minecraft:sand", {});
	std::uint64_t snow = get_block("minecraft:snow", {});
	std::uint64_t ice = get_block("minecraft:ice", {});
	std::uint64_t grass_block_snowy = get_block("minecraft:grass_block", {{"snowy", json_value("true")}});
	std::uint64_t short_dry_grass = get_block("minecraft:short_dry_grass", {});
	std::uint64_t tall_dry_grass = get_block("minecraft:tall_dry_grass", {});
	std::uint64_t dead_bush = get_block("minecraft:dead_bush", {});

	for (auto &sec: sections)
	{
		sec.biome_palette[0] = std::distance(biomes.begin(), std::find(biomes.begin(), biomes.end(), "plains"));
		sec.biome_palette.push_back(std::distance(biomes.begin(), std::find(biomes.begin(), biomes.end(), "snowy_plains")));
	}

	for (int i = 0; i < 4; i++)
	{
		for (int xx = 0; xx < 4; xx++)
		{
			int world_x = x * 16 + (xx * 4);
			int world_z = z * 16 + (i * 4);
			const double temp = temperature.octave2D_11(world_x * 0.0005221649073064327, world_z * 0.000522164907306432, 16);
			if (temp >= 0.3f)
			{
				for (auto &sec: sections)
				{
					sec.biome_palette[1] = std::distance(biomes.begin(), std::find(biomes.begin(), biomes.end(), "desert"));
				}
			}	
			for (int y = 0; y < 96; y++)
			{
				int section_index = ((y * 4))/16;
				if (temp < -0.3f)
					sections[section_index].biome[(rem_euclid(y, 4) * 16) + (i * 4) + xx] = 1;
				else if (temp < 0.3f)
					sections[section_index].biome[(rem_euclid(y, 4) * 16) + (i * 4) + xx] = 0;
				else
				 	sections[section_index].biome[(rem_euclid(y, 4) * 16) + (i * 4) + xx] = 1;
			}
		}
	}

	for (int z_ = 0; z_ < 16; z_++)
	{
		for (int x_ = 0; x_ < 16; x_++)
		{

			int world_x = x * 16 + x_;
			int world_z = z * 16 + z_;

			const double value = continentality_noise.octave2D_11(world_x * 0.003221649073064327, world_z * 0.00322164907306432, 6);

			y_max = continentalness.get_value(value);
			
			const double temp = temperature.octave2D_11(world_x * 0.0005221649073064327, world_z * 0.000522164907306432, 16);
			
			if (world_x == 0 && world_z == 0)
				*spawn_y = y_max;
			float squish = -0.2f;
			if (temp < -0.3f)
			{
				for (int y = -64; y < y_max; y++)
				{
					const double cave_val = cave_noise.octave3D(world_x * 0.041f, y * 0.05f, world_z * 0.041f, 2);
					if (cave_val > -0.48f - squish)
					{
						if (y < y_max - 3)
							set_block_direct(x_, y, z_, stone);
						else if (y < y_max - 1)
							auto ret = set_block(x_, y, z_, dirt);
						else if (y_max >= 64)
						{
							auto ret = set_block(x_, y, z_, grass_block_snowy);
							auto r = set_block(x_, y + 1, z_, snow);
						}
					
						if (y == y_max - 1 && y_max >= 64)
						{
							const double bush_val = bush_noise.octave2D(world_x * 2, world_z * 2, 2);
							if (bush_val > 0.5f)
							{
								auto r = set_block(x_, y, z_, grass_id);
								auto ret = set_block(x_, y + 1, z_, short_grass);
							}
							const double tree_val = tree_noise.noise2D(world_x * 1.5, world_z * 1.5);
							if (tree_val > 0.7f)
								trees.emplace_back(position_int(world_x, y, world_z), TREE_TYPE::TAIGA);
						}
					}
					squish += 0.002f;
				}
				if (y_max < 64)
				{
					for (int y = y_max - 1; y < 64; y++)
					{
						if (y == 63)
						{
							if (!set_block(x_, y, z_, ice))
								log("Set block failed!", LOG_LEVEL::ERROR);
						}
						else
						{
							if (!set_block(x_, y, z_, water))
								log("Set block failed!", LOG_LEVEL::ERROR);
						}
					}
				}
			}
			else if (temp < 0.3f)
			{
				for (int y = -64; y < y_max; y++)
				{
					const double cave_val = cave_noise.octave3D(world_x * 0.041f, y * 0.05f, world_z * 0.041f, 2);
					if (cave_val > -0.48f - squish)
					{
						if (y < y_max - 3)
							set_block_direct(x_, y, z_, stone);
						else if (y < y_max - 1)
							auto ret = set_block(x_, y, z_, dirt);
						else if (y_max >= 64)
							auto ret = set_block(x_, y, z_, grass_id);
					

						if (y == y_max - 1 && y_max >= 64)
						{
							const double bush_val = bush_noise.noise2D(world_x, world_z);
							if (bush_val > 0.41f)
							{
								auto ret = set_block(x_, y + 1, z_, tall_grass_lower);
								auto ret2 = set_block(x_, y + 2, z_, tall_grass_upper);
							}
							else if (bush_val > 0.0f)
								auto ret = set_block(x_, y + 1, z_, short_grass);

							const double tree_val = tree_noise.noise2D(world_x * 1.5, world_z * 1.5);
							if (tree_val > 0.6f)
								trees.emplace_back(position_int(world_x, y, world_z), TREE_TYPE::OAK);
						}
					}
					squish += 0.002f;
				}
				if (y_max < 64)
				{
					for (int y = y_max - 1; y < 64; y++)
					{
						if (!set_block(x_, y, z_, water))
							log("Set block failed!", LOG_LEVEL::ERROR);
					}
				}
			}
			else
			{
				for (int y = -64; y < y_max; y++)
				{
					const double cave_val = cave_noise.octave3D(world_x * 0.041f, y * 0.05f, world_z * 0.041f, 2);
					if (cave_val > -0.48f - squish)
					{
						if (y < y_max - 3)
							set_block_direct(x_, y, z_, stone);
						else if (y < y_max - 1)
							auto ret = set_block(x_, y, z_, dirt);
						else if (y_max >= 64)
							auto ret = set_block(x_, y, z_, sand);
					
						if (y == y_max - 1 && y_max >= 64)
						{
							const double bush_val = bush_noise.octave2D(world_x * 2, world_z * 2, 2);
							if (bush_val > 0.6f)
							{
								auto ret = set_block(x_, y + 1, z_, dead_bush);
							}
							else if (bush_val > 0.55f)
							{
								auto ret = set_block(x_, y + 1, z_, tall_dry_grass);
							}
							else if (bush_val > 0.5f)
							{
								auto ret = set_block(x_, y + 1, z_, short_dry_grass);
							}
							const double tree_val = tree_noise.noise2D(world_x * 1.5, world_z * 1.5);
							if (tree_val > 0.7f)
								trees.emplace_back(position_int(world_x, y, world_z), TREE_TYPE::CACTUS);
						}
					}
					squish += 0.002f;
				}
				if (y_max < 64)
				{
					for (int y = y_max - 1; y < 64; y++)
					{
						if (!set_block(x_, y, z_, water))
							log("Set block failed!", LOG_LEVEL::ERROR);
					}
				}
			}
		}
	}
	const ms duration = clock::now() - before;
	log(std::format("Chunk generation took {}", duration), LOG_LEVEL::NORMAL);
}


chunk & world::get_chunk(int x, int z)
{
	auto ret = chunks.find(std::make_pair(x, z));
	if (ret == chunks.end())
	{
		auto &chunk = chunks.emplace(std::piecewise_construct, std::forward_as_tuple(x, z), std::forward_as_tuple(x, z)).first->second;
		chunk.generate(trees_to_build, continentality_noise, main_noise, bush_noise, tree_noise, temperature, cave_noise, biomes);
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
		chunk.generate(trees_to_build, continentality_noise, main_noise, bush_noise, tree_noise, temperature, cave_noise, biomes,spawn_y);
		return chunk;
	}
	return ret->second;
}

void world::generate_seeds()
{
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> dist(1, UINT32_MAX);

	continentality_seed = dist(rng);
	erosion_seed = dist(rng);
	bush_seed = dist(rng);
	tree_seed = dist(rng);
	temperature_seed = dist(rng);
	cave_seed = dist(rng);

	main_noise = siv::PerlinNoise {erosion_seed};
	continentality_noise = siv::PerlinNoise {continentality_seed};
	bush_noise = siv::PerlinNoise {bush_seed};
	tree_noise = siv::PerlinNoise {tree_seed};
	temperature = siv::PerlinNoise {tree_seed};
	cave_noise = siv::PerlinNoise {tree_seed};
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

void world::set_block_direct(int x, int y, int z, std::uint64_t b)
{
	chunk &c = get_chunk(floor((float)x/16.0f), floor((float)z/16.0f));
	auto ret = c.set_block(rem_euclid(x, 16), y, rem_euclid(z, 16), b);
	if (!ret)
	{
		log("Block placement failed", LOG_LEVEL::ERROR);
		//return std::unexpected(ret.error());
	}
}

void world::set_blocks(std::map<std::string, block> b)
{
	blocks_ = std::move(b);
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
		if (pos.type == TREE_TYPE::CACTUS)
		{
			std::uint64_t cactus = get_block("minecraft:cactus", {});
			std::random_device dev;
			std::mt19937 rng(dev());
			std::uniform_int_distribution<std::mt19937::result_type> dist(1, 4);
			int cactus_size = dist(rng);
			for (int y = pos.pos.y + 1; y < (pos.pos.y + cactus_size); y++)
			{
				set_block(pos.pos.x, y, pos.pos.z, cactus);
			}
			continue;
		}
		else if (pos.type == TREE_TYPE::TAIGA)
		{
			log_id = get_block("minecraft:spruce_log", {});
			set_block_direct(pos.pos.x, pos.pos.y + 1, pos.pos.z, log_id);
			
			for (int z = -3; z <=3; z++)
			{
				for(int x = -3; x <= 3; x++)
				{
					if (x == 0 && z == 0)
						set_block_direct(pos.pos.x + x, pos.pos.y + 2, pos.pos.z + z, log_id);
					else if (!(x == 3 && z == 3) && !(x == 3 && z == -3) && !(z == -3 && x == -3) && !(z == 3 && x == -3))
						set_block_direct(pos.pos.x + x, pos.pos.y + 2, pos.pos.z + z, leaves_id);
				}
			}
			for (int z = -2; z <= 2; z++)
			{
				for(int x = -2; x <= 2; x++)
				{
					if (x == 0 && z == 0)
						set_block_direct(pos.pos.x + x, pos.pos.y + 3, pos.pos.z + z, log_id);
					else if (!(x == 2 && z == 2) && !(x == 2 && z == -2) && !(z == -2 && x == -2) && !(z == 2 && x == -2))
						set_block_direct(pos.pos.x + x, pos.pos.y + 3, pos.pos.z + z, leaves_id);
				}
			}
			set_block_direct(pos.pos.x + 1, pos.pos.y + 4, pos.pos.z, leaves_id);
			set_block_direct(pos.pos.x - 1, pos.pos.y + 4, pos.pos.z, leaves_id);
			set_block_direct(pos.pos.x, pos.pos.y + 4, pos.pos.z + 1, leaves_id);
			set_block_direct(pos.pos.x, pos.pos.y + 4, pos.pos.z - 1, leaves_id);
			set_block_direct(pos.pos.x, pos.pos.y + 4, pos.pos.z, log_id);

			for (int z = -2; z <= 2; z++)
			{
				for(int x = -2; x <= 2; x++)
				{
					if (x == 0 && z == 0)
						set_block_direct(pos.pos.x + x, pos.pos.y + 5, pos.pos.z + z, log_id);
					else if (!(x == 2 && z == 2) && !(x == 2 && z == -2) && !(z == -2 && x == -2) && !(z == 2 && x == -2))
						set_block_direct(pos.pos.x + x, pos.pos.y + 5, pos.pos.z + z, leaves_id);
				}
			}

			set_block_direct(pos.pos.x + 1, pos.pos.y + 6, pos.pos.z, leaves_id);
			set_block_direct(pos.pos.x - 1, pos.pos.y + 6, pos.pos.z, leaves_id);
			set_block_direct(pos.pos.x, pos.pos.y + 6, pos.pos.z + 1, leaves_id);
			set_block_direct(pos.pos.x, pos.pos.y + 6, pos.pos.z - 1, leaves_id);
			set_block_direct(pos.pos.x, pos.pos.y + 6, pos.pos.z, log_id);

			set_block_direct(pos.pos.x, pos.pos.y + 7, pos.pos.z, log_id);

			set_block_direct(pos.pos.x + 1, pos.pos.y + 8, pos.pos.z, leaves_id);
			set_block_direct(pos.pos.x - 1, pos.pos.y + 8, pos.pos.z, leaves_id);
			set_block_direct(pos.pos.x, pos.pos.y + 8, pos.pos.z + 1, leaves_id);
			set_block_direct(pos.pos.x, pos.pos.y + 8, pos.pos.z - 1, leaves_id);
			set_block_direct(pos.pos.x, pos.pos.y + 8, pos.pos.z, leaves_id);
			std::random_device dev;
			std::mt19937 rng(dev());
			std::uniform_int_distribution<std::mt19937::result_type> dist(0, 4);
			if (dist(rng) >= 2)
				set_block_direct(pos.pos.x, pos.pos.y + 9, pos.pos.z, leaves_id);
			continue;
		}
		set_block(pos.pos.x, pos.pos.y + 1, pos.pos.z, log_id);
		set_block(pos.pos.x, pos.pos.y + 2, pos.pos.z, log_id);
		for (int y = 3; y < 5; y++)
		{
			for(int z = -3; z <= 3; z++)
			{
				for(int x = -3; x <= 3; x++)
				{
					if (x == 0 && z == 0)
						set_block(pos.pos.x + x, pos.pos.y + y, pos.pos.z + z, log_id);
					else
						set_block(pos.pos.x + x, pos.pos.y + y, pos.pos.z + z, leaves_id);
				}
			}
		}
		for(int z = -2; z <= 2; z++)
		{
			for(int x = -2; x <= 2; x++)
			{
				if (x == 0 && z == 0)
					set_block(pos.pos.x + x, pos.pos.y + 5, pos.pos.z + z, log_id);
				else
					set_block(pos.pos.x + x, pos.pos.y + 5, pos.pos.z + z, leaves_id);
			}
		}
		set_block(pos.pos.x + 1, pos.pos.y + 6, pos.pos.z, leaves_id);
		set_block(pos.pos.x - 1, pos.pos.y + 6, pos.pos.z, leaves_id);
		set_block(pos.pos.x, pos.pos.y + 6, pos.pos.z, leaves_id);
		set_block(pos.pos.x, pos.pos.y + 6, pos.pos.z + 1, leaves_id);
		set_block(pos.pos.x, pos.pos.y + 6, pos.pos.z - 1, leaves_id);
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
