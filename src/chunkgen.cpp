#include "chunkgen.h"
spline continentalness = {.dots = {{.start_noise_val = -1.0f, .end_noise_val = -0.8f, .start_value = 10, .end_value = 40},
								   {.start_noise_val = -0.8f, .end_noise_val = -0.5f, .start_value = 40, .end_value = 45},
								   {.start_noise_val = -0.5f, .end_noise_val = -0.3f, .start_value = 45, .end_value = 64},
								   {.start_noise_val = -0.3f, .end_noise_val = 0.5f, .start_value = 64, .end_value = 90},
								   {.start_noise_val = 0.5f, .end_noise_val = 0.7f, .start_value = 90, .end_value = 120},
								   {.start_noise_val = 0.7f, .end_noise_val = 0.9f, .start_value = 120, .end_value = 170},
								   {.start_noise_val = 0.9f, .end_noise_val = 1.01f, .start_value = 170, .end_value = 250}}};

static void *memdup(void *dat, size_t size)
{
	char *cpy = (char *)malloc(size);

	memcpy(cpy, dat, size);
	return cpy;
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

int8_t *chunk_generator::generate(int x, int z, int *spawn_y)
{
    using clock = std::chrono::system_clock;
	using ms = std::chrono::duration<double, std::milli>;
	

	const auto before = clock::now();
	size_t size = 4096 * 24;
	int y_max = 64;

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
	int8_t *ret_data = (int8_t *)memdup(out_data, size);
	memset(out_data, 0, size);
	device.unmapMemory(buffer_out_memory);
	device.destroyFence(fence);
	device.destroyCommandPool(command_pool);
	const ms duration = clock::now() - before;
	log(std::format("Chunk generation took {}", duration), LOG_LEVEL::NORMAL);
	return ret_data;
}

void chunk_generator::init()
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
	size_t size = 4096 * 24;
	auto buffer_out_ret_ret = create_buffer(device, physical_device, vk::BufferUsageFlagBits::eStorageBuffer, size);
	if (!buffer_out_ret_ret)
	{
		std::println("{}", buffer_out_ret_ret.error());
		return ;
	}

	auto buffer_out_ret = buffer_out_ret_ret.value();
	buffer_out_memory = buffer_out_ret.first;
	buffer_out = buffer_out_ret.second;


	const std::vector<vk::DescriptorSetLayoutBinding> descriptor_set_layout_binding = {
		{0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
	};
	vk::DescriptorSetLayoutCreateInfo descriptor_layout_info(vk::DescriptorSetLayoutCreateFlags(), descriptor_set_layout_binding);
	vk::DescriptorSetLayout descriptor_set_layout = device.createDescriptorSetLayout(descriptor_layout_info);

	vk::PushConstantRange push_range(vk::ShaderStageFlagBits::eCompute, 0, sizeof(parameters));

	vk::PipelineLayoutCreateInfo pipeline_layout_info(vk::PipelineLayoutCreateFlags(), descriptor_set_layout, push_range);
	pipeline_layout = device.createPipelineLayout(pipeline_layout_info);
	vk::PipelineCache pipeline_cache = device.createPipelineCache(vk::PipelineCacheCreateInfo());

	vk::PipelineShaderStageCreateInfo pipeline_shader_info(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, shader_module, "main");
	vk::ComputePipelineCreateInfo compute_pipeline_info(vk::PipelineCreateFlags(), pipeline_shader_info, pipeline_layout);
	pipeline = device.createComputePipeline(pipeline_cache, compute_pipeline_info).value;

	vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1);
	vk::DescriptorPoolCreateInfo descriptor_pool_info(vk::DescriptorPoolCreateFlags(), 1, descriptor_pool_size);
	vk::DescriptorPool descriptor_pool = device.createDescriptorPool(descriptor_pool_info);

	vk::DescriptorSetAllocateInfo descriptor_alloc_info(descriptor_pool, 1, &descriptor_set_layout);
	descriptor_sets = device.allocateDescriptorSets(descriptor_alloc_info);
	vk::DescriptorBufferInfo buffer_out_info(buffer_out, 0, size);
	std::vector<vk::WriteDescriptorSet> write_descriptor_sets =
	{
		{descriptor_sets[0], 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &buffer_out_info}
	};
	device.updateDescriptorSets(write_descriptor_sets, {});
}