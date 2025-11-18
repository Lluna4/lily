#pragma once
#include <vector>
#include <map>
#include "user.h"
#include "chunkgen.h"


struct world
{
	std::map<std::pair<int, int>, chunk> chunks;

	void set_blocks(std::map<std::string, block> blocks);
	chunk &get_chunk(int x, int z);
	void generate_seeds();
	std::expected<bool, chunk_error> set_block(int x, int y, int z, std::uint64_t b);
	void build_trees();
	json_value get_block_properties(std::string block);
	std::uint64_t get_block(std::string block, std::map<std::string, json_value> properties);
	std::uint64_t get_block(int x, int y, int z);
	bool is_id_block(std::uint64_t id, std::vector<std::string>);
	void generate(std::vector<position_int> positions, int *spawn_y = nullptr);
	chunk_generator chunkgen;
};