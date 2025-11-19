#pragma once
#include "PerlinNoise.hpp"
#include <vulkan/vulkan.hpp>
#include <chrono>
#include <print>
#include <expected>
#include "log.h"
#include "user.h"
#include "chunk.h"

struct parameters
{
	int x;
	int z;
	int chunk;
};

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


struct chunk_generator
{
    std::vector<position_int> trees_to_build;
	siv::PerlinNoise::seed_type continentality_seed;
	siv::PerlinNoise::seed_type erosion_seed;
	siv::PerlinNoise::seed_type bush_seed;
	siv::PerlinNoise::seed_type tree_seed;
	siv::PerlinNoise continentality_noise;
	siv::PerlinNoise main_noise;
	siv::PerlinNoise bush_noise;
	siv::PerlinNoise tree_noise;
	vk::Device device;
	vk::PhysicalDevice physical_device;
	vk::ShaderModule shader_module;
	std::map<std::string, block> blocks;
	parameters params;
	int queue_family_index;
	std::vector<chunk> generate_mult(std::vector<position_int> positions, int *spawn_y = nullptr);
	void init();
};
