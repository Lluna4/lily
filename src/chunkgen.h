#pragma once
#include "PerlinNoise.hpp"
#include <vulkan/vulkan.hpp>
#include <chrono>
#include <print>
#include <expected>
#include "log.h"
#include "user.h"

struct parameters
{
	int y_max;
	int x;
	int z;
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
    std::vector<vk::DescriptorSet> descriptor_sets;
    vk::Pipeline pipeline;
	vk::DeviceMemory buffer_out_memory;
	vk::Buffer buffer_out;
	vk::PipelineLayout pipeline_layout;
	int queue_family_index;
    int8_t *generate(int x, int z, int *spawn_y = nullptr);
	void init();
};
