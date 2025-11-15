#include "chunk.h"
#include "json_reader.h"
#include "log.h"
#include <cstdint>

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

enum class Error
{
	NO_SUITABLE_QUEUE_FAMILY_FOUND,
	NO_SUITABLE_MEMORY_FOUND
};


template <>
struct std::formatter<Error>
{

	constexpr auto parse(std::format_parse_context& ctx) {
		return ctx.begin();
	}

	auto format(const Error& id, std::format_context& ctx) const
	{
		if (id == Error::NO_SUITABLE_QUEUE_FAMILY_FOUND)
			return std::format_to(ctx.out(), "{}", "A suitable queue family has not been found");
		if (id == Error::NO_SUITABLE_MEMORY_FOUND)
			return std::format_to(ctx.out(), "{}", "A suitable memory type has not been found");
		return std::format_to(ctx.out(), "{}", "Unknown error");
	}
};

vk::PhysicalDevice get_physical_device(vk::Instance &instance)
{
	std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
	std::vector<int> scores;
	for (vk::PhysicalDevice &device: devices) // score is only representative for compute
	{
		vk::PhysicalDeviceProperties device_propierties = device.getProperties();

		scores.push_back((device_propierties.limits.maxComputeSharedMemorySize + device_propierties.limits.maxComputeWorkGroupCount.front() + device_propierties.limits.maxComputeWorkGroupInvocations + device_propierties.limits.maxComputeWorkGroupSize.front())/4);
	}

	int max_index = 0;
	for (int i = 1; i < scores.size(); i++)
	{
		if (scores[i] > scores[max_index])
			max_index = i;
	}

	std::println("Max score {}", scores[max_index]);
	return devices[max_index];
}

std::expected<int,Error> get_compute_queue_family_index(vk::PhysicalDevice &physical_device)
{
	std::vector<vk::QueueFamilyProperties> queue_propierties = physical_device.getQueueFamilyProperties();
	int index = 0;

	for (int i = 0; i < queue_propierties.size(); i++)
	{
		if (queue_propierties[i].queueFlags & vk::QueueFlagBits::eCompute)
		{
			return i;
		}
	}
	return std::unexpected(Error::NO_SUITABLE_QUEUE_FAMILY_FOUND);
}

std::expected<std::pair<vk::DeviceMemory, vk::Buffer>, Error> create_buffer(const vk::Device &device, vk::PhysicalDevice selected_physical_device, vk::BufferUsageFlagBits usage, size_t size)
{
    vk::BufferCreateInfo buffer_info = vk::BufferCreateInfo(vk::BufferCreateFlags(), size, usage, vk::SharingMode::eExclusive);
    vk::Buffer vertex_buffer = device.createBuffer(buffer_info);
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, vertex_buffer, &memory_requirements);
    vk::PhysicalDeviceMemoryProperties memory_properties = selected_physical_device.getMemoryProperties();

    int propierty_index = -1;
    for (int i = 0; i < memory_properties.memoryTypeCount; i++)
    {
        if (memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached))
        {
            propierty_index = i;
            break;
        }
    }
    if (propierty_index == -1)
    {
		return std::unexpected(Error::NO_SUITABLE_MEMORY_FOUND);
    }

    vk::MemoryAllocateInfo alloc_info = vk::MemoryAllocateInfo(memory_requirements.size, propierty_index);

    vk::DeviceMemory vertex_buffer_memory = device.allocateMemory(alloc_info);
    device.bindBufferMemory(vertex_buffer, vertex_buffer_memory, 0);
    return std::make_pair(vertex_buffer_memory, vertex_buffer);
}

std::string get_file_contents(std::string filename)
{
	std::ifstream file;
	file.open(filename);
	std::stringstream reader;
	reader << file.rdbuf();
	return reader.str();
}

void chunk::generate(std::vector<position_int> &trees, siv::PerlinNoise &continentality_noise, siv::PerlinNoise &main_noise, siv::PerlinNoise &bush_noise, siv::PerlinNoise &tree_noise, vk::Device &device, int queue_family_index,vk::PhysicalDevice &physical_device, vk::ShaderModule &shader_module,int *spawn_y)
{
	using clock = std::chrono::system_clock;
	using ms = std::chrono::duration<double, std::milli>;

	const auto before = clock::now();
	int y_max = 64;
	std::uint64_t dirt = get_block("minecraft:dirt", {});
	std::uint64_t grass_id = get_block("minecraft:grass_block", {});
	std::uint64_t tall_grass_lower = get_block("minecraft:tall_grass", {});
	std::uint64_t tall_grass_upper = get_block("minecraft:tall_grass", {{"half", json_value("upper")}});
	std::uint64_t water = get_block("minecraft:water", {});
	std::uint64_t short_grass = get_block("minecraft:short_grass", {});

	size_t size = 4096 * 24;
	auto buffer_out_ret_ret = create_buffer(device, physical_device, vk::BufferUsageFlagBits::eStorageBuffer, size);
	if (!buffer_out_ret_ret)
	{
		std::println("{}", buffer_out_ret_ret.error());
		return ;
	}

	auto buffer_out_ret = buffer_out_ret_ret.value();
	vk::DeviceMemory buffer_out_memory = buffer_out_ret.first;
	vk::Buffer buffer_out = buffer_out_ret.second;


	const std::vector<vk::DescriptorSetLayoutBinding> descriptor_set_layout_binding = {
		{0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
	};
	vk::DescriptorSetLayoutCreateInfo descriptor_layout_info(vk::DescriptorSetLayoutCreateFlags(), descriptor_set_layout_binding);
	vk::DescriptorSetLayout descriptor_set_layout = device.createDescriptorSetLayout(descriptor_layout_info);

	vk::PushConstantRange push_range(vk::ShaderStageFlagBits::eCompute, 0, sizeof(parameters));

	vk::PipelineLayoutCreateInfo pipeline_layout_info(vk::PipelineLayoutCreateFlags(), descriptor_set_layout, push_range);
	vk::PipelineLayout pipeline_layout = device.createPipelineLayout(pipeline_layout_info);
	vk::PipelineCache pipeline_cache = device.createPipelineCache(vk::PipelineCacheCreateInfo());

	vk::PipelineShaderStageCreateInfo pipeline_shader_info(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, shader_module, "main");
	vk::ComputePipelineCreateInfo compute_pipeline_info(vk::PipelineCreateFlags(), pipeline_shader_info, pipeline_layout);
	vk::Pipeline pipeline = device.createComputePipeline(pipeline_cache, compute_pipeline_info).value;

	vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1);
	vk::DescriptorPoolCreateInfo descriptor_pool_info(vk::DescriptorPoolCreateFlags(), 1, descriptor_pool_size);
	vk::DescriptorPool descriptor_pool = device.createDescriptorPool(descriptor_pool_info);

	vk::DescriptorSetAllocateInfo descriptor_alloc_info(descriptor_pool, 1, &descriptor_set_layout);
	std::vector<vk::DescriptorSet> descriptor_sets = device.allocateDescriptorSets(descriptor_alloc_info);
	vk::DescriptorBufferInfo buffer_out_info(buffer_out, 0, size);
	std::vector<vk::WriteDescriptorSet> write_descriptor_sets =
	{
		{descriptor_sets[0], 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &buffer_out_info}
	};
	device.updateDescriptorSets(write_descriptor_sets, {});

	vk::CommandPoolCreateInfo command_pool_info(vk::CommandPoolCreateFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer), queue_family_index);
	vk::CommandPool command_pool = device.createCommandPool(command_pool_info);

	vk::CommandBufferAllocateInfo command_buffer_alloc_info(command_pool, vk::CommandBufferLevel::ePrimary, 1);
	std::vector<vk::CommandBuffer> command_buffers = device.allocateCommandBuffers(command_buffer_alloc_info);

	vk::CommandBuffer command_buffer = command_buffers.front();
	vk::Fence fence = device.createFence(vk::FenceCreateInfo());
	vk::Queue queue = device.getQueue(queue_family_index, 0);
	vk::CommandBufferBeginInfo command_buffer_begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	command_buffer.begin(command_buffer_begin_info);
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_sets[0]}, {});
	parameters params{};
	for (int z_ = 0; z_ < 16; z_++)
	{
		for (int x_ = 0; x_ < 16; x_++)
		{
			int world_x = x * 16 + x_;
			int world_z = z * 16 + z_;

			const double value = continentality_noise.octave2D_11(world_x * 0.003221649073064327, world_z * 0.00322164907306432, 6);

			y_max = continentalness.get_value(value);
			
			
			/*double bias = 0.0f;

			if (y_max_max > 64)
				bias += y_max_max * 0.00284810126;
			for (int y = y_max_max; y > -64; y--)
			{
				const double value = main_noise.octave3D(world_x * 0.005221649073064327, y * 0.0026108245365321636 ,world_z * 0.005221649073064327, 16);
				if (value > (0.0f + bias))
				{
					y_max = y;
					break;
				}
				bias-= 0.00284810126;
			}*/
			if (world_x == 0 && world_z == 0)
				*spawn_y = y_max;

			params.y_max = y_max;
			params.x = x_;
			params.z = z_;

			command_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(parameters), &params);
			command_buffer.dispatch(y_max + 1 + 64, 1, 1);
		}
	}
	command_buffer.end();
	vk::SubmitInfo submit_info(0, nullptr, nullptr, 1, &command_buffer);
	queue.submit({submit_info}, fence);
	auto res = device.waitForFences({fence}, true, -1);
	int8_t *out_data = (int8_t *)device.mapMemory(buffer_out_memory, 0, size);
	for (int i = 0; i < sections.size(); i++)
	{
		int index = i * 4096;
		std::memcpy(sections[i].blocks.data(), &out_data[index], 4096);
		sections[i].non_air_blocks = 4096;
	}

	device.unmapMemory(buffer_out_memory);
	device.destroyFence(fence);
	device.destroyCommandPool(command_pool);
	device.destroyPipelineLayout(pipeline_layout);
	device.destroyPipelineCache(pipeline_cache);
	device.destroyPipeline(pipeline);
	device.destroyDescriptorPool(descriptor_pool);
	device.destroyDescriptorSetLayout(descriptor_set_layout);
	device.freeMemory(buffer_out_memory);
	device.destroyBuffer(buffer_out);
	const ms duration = clock::now() - before;
	log(std::format("Chunk generation took {}", duration), LOG_LEVEL::NORMAL);
}


chunk & world::get_chunk(int x, int z)
{
	auto ret = chunks.find(std::make_pair(x, z));
	if (ret == chunks.end())
	{
		auto &chunk = chunks.emplace(std::piecewise_construct, std::forward_as_tuple(x, z), std::forward_as_tuple(x, z)).first->second;
		chunk.generate(trees_to_build, continentality_noise, main_noise, bush_noise, tree_noise, device, queue_family_index, physical_device, shader_module);
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
		chunk.generate(trees_to_build, continentality_noise, main_noise, bush_noise, tree_noise, device, queue_family_index, physical_device, shader_module, spawn_y);
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

	main_noise = siv::PerlinNoise {erosion_seed};
	continentality_noise = siv::PerlinNoise {continentality_seed};
	bush_noise = siv::PerlinNoise {bush_seed};
	tree_noise = siv::PerlinNoise {tree_seed};

		vk::ApplicationInfo app_info("Test_compute", 1, nullptr, 0, VK_API_VERSION_1_4);

	#ifdef NDEBUG
	std::vector<const char *> layers = {};
	#else
	std::vector<const char *> layers = {"VK_LAYER_KHRONOS_validation"};
	std::println("Debug mode");
	#endif
	#ifdef __APPLE__
	std::vector<const char *> extensions = {"VK_KHR_portability_enumeration", VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
	#else
	std::vector<const char *> extensions = {};
	#endif
	vk::InstanceCreateInfo instance_info(vk::InstanceCreateFlags(VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR), &app_info, layers.size(), layers.data(), extensions.size(), extensions.data());

	vk::Instance instance = vk::createInstance(instance_info);

	physical_device = get_physical_device(instance);

	std::println("Selected device: {}", physical_device.getProperties().deviceName.data());

	auto queue_family_index_ret = get_compute_queue_family_index(physical_device);

	if (!queue_family_index_ret)
	{
		std::println("{}", queue_family_index_ret.error());
		return ;
	}
	queue_family_index = queue_family_index_ret.value();

	std::println("queue family index is {}", queue_family_index);

	float queue_priority = 1.0f;
	vk::DeviceQueueCreateInfo device_queue_info(vk::DeviceQueueCreateFlags(), queue_family_index, 1, &queue_priority);

	#ifdef __APPLE__
	std::vector<const char *> device_extensions = {"VK_KHR_portability_subset", "VK_KHR_8bit_storage", "VK_KHR_shader_float16_int8"};
	#else
	std::vector<const char *> device_extensions = {"VK_KHR_8bit_storage", "VK_KHR_shader_float16_int8"};
	#endif
	vk::PhysicalDeviceFeatures device_features = vk::PhysicalDeviceFeatures();
	vk::PhysicalDeviceVulkan12Features vulkan_1_2_features = vk::PhysicalDeviceVulkan12Features();
	vulkan_1_2_features.shaderInt8 = VK_TRUE;
	vulkan_1_2_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
	vk::DeviceCreateInfo device_info(vk::DeviceCreateFlags(), 1, &device_queue_info, 0, nullptr, device_extensions.size(), device_extensions.data(), &device_features);
	device_info.setPNext(&vulkan_1_2_features);
	device = physical_device.createDevice(device_info);
	std::string compiled_code = get_file_contents("../shaders/a.spv");
	vk::ShaderModuleCreateInfo shader_info(vk::ShaderModuleCreateFlags(), compiled_code.size(), reinterpret_cast<const uint32_t*>(compiled_code.c_str()));
	shader_module = device.createShaderModule(shader_info);
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

void world::set_blocks(std::map<std::string, block> b)
{
	blocks_ = b;
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
		set_block(pos.x, pos.y + 1, pos.z, log_id);
		set_block(pos.x, pos.y + 2, pos.z, log_id);
		for (int y = 3; y < 5; y++)
		{
			for(int z = -3; z <= 3; z++)
			{
				for(int x = -3; x <= 3; x++)
				{
					if (x == 0 && z == 0)
						set_block(pos.x + x, pos.y + y, pos.z + z, log_id);
					else
						set_block(pos.x + x, pos.y + y, pos.z + z, leaves_id);
				}
			}
		}
		for(int z = -2; z <= 2; z++)
		{
			for(int x = -2; x <= 2; x++)
			{
				if (x == 0 && z == 0)
					set_block(pos.x + x, pos.y + 5, pos.z + z, log_id);
				else
					set_block(pos.x + x, pos.y + 5, pos.z + z, leaves_id);
			}
		}
		set_block(pos.x + 1, pos.y + 6, pos.z, leaves_id);
		set_block(pos.x - 1, pos.y + 6, pos.z, leaves_id);
		set_block(pos.x, pos.y + 6, pos.z, leaves_id);
		set_block(pos.x, pos.y + 6, pos.z + 1, leaves_id);
		set_block(pos.x, pos.y + 6, pos.z - 1, leaves_id);
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
