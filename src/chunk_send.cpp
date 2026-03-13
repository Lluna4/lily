#include "chunk_send.h"
#include "log.h"
#include <string_view>

std::mutex world_mut;
std::vector<chunk_request> chunks_to_send;
std::condition_variable notify_send;
bool thread = true;

struct parameters
{
	int x;
	int z;
	int chunk;
    float seed;
};


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


void send_chunks(int fd, std::vector<std::pair<int, int>> &pos)
{
	std::unique_lock lock(world_mut);
	for (auto &p: pos)
	{
		chunks_to_send.emplace_back(p.first, p.second, fd);
	}
	lock.unlock();
	notify_send.notify_all();
}

vk::PhysicalDevice get_physical_device(vk::Instance &instance)
{
	std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    log(std::format("Device count {}", devices.size()), LOG_LEVEL::NORMAL);
	std::vector<int> scores;
	for (vk::PhysicalDevice &device: devices) // score is only representative for compute
	{
		vk::PhysicalDeviceProperties device_propierties = device.getProperties();

		scores.push_back((device_propierties.limits.maxComputeSharedMemorySize + device_propierties.limits.maxComputeWorkGroupCount.front() + device_propierties.limits.maxComputeWorkGroupInvocations + device_propierties.limits.maxComputeWorkGroupSize.front())/4);
	}

	int max_index = 0;
	for (int i = 1; i < scores.size(); i++)
	{
		if (scores[i] > scores[max_index] && !std::string_view(devices[i].getProperties().deviceName.data()).contains("llvmpipe"))
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


std::string get_file_contents(std::string filename)
{
	std::ifstream file;
	file.open(filename);
	std::stringstream reader;
	reader << file.rdbuf();
	return reader.str();
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
		vk::MemoryPropertyFlags required_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCached | vk::MemoryPropertyFlagBits::eHostCoherent;

       	if ((memory_properties.memoryTypes[i].propertyFlags & required_flags) == required_flags)
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



void world_thread(server &sv, world &w)
{
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> dist(1, UINT32_MAX);
    siv::PerlinNoise::seed_type continentality_seed;
	siv::PerlinNoise::seed_type erosion_seed;
	siv::PerlinNoise::seed_type bush_seed;
	siv::PerlinNoise::seed_type tree_seed;
	siv::PerlinNoise::seed_type temperature_seed;
	siv::PerlinNoise::seed_type cave_seed;

	continentality_seed = dist(rng);
	erosion_seed = dist(rng);
	bush_seed = dist(rng);
	tree_seed = dist(rng);
	temperature_seed = dist(rng);
	cave_seed = dist(rng);


	vk::ApplicationInfo app_info("Test_compute", 1, nullptr, 0, VK_API_VERSION_1_4);

	#ifdef NDEBUG
	std::vector<const char *> layers = {};
	#else
	std::vector<const char *> layers = {"VK_LAYER_KHRONOS_validation"};
	std::println("Debug mode");
	#endif
	#ifdef __APPLE__
	std::vector<const char *> extensions = {"VK_KHR_portability_enumeration", VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
	vk::InstanceCreateInfo instance_info(vk::InstanceCreateFlags(VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR), &app_info, layers.size(), layers.data(), extensions.size(), extensions.data());
	#else
	std::vector<const char *> extensions = {};
	vk::InstanceCreateInfo instance_info(vk::InstanceCreateFlags(), &app_info, layers.size(), layers.data(), extensions.size(), extensions.data());
	#endif
	vk::Instance instance = vk::createInstance(instance_info);

	vk::PhysicalDevice physical_device = get_physical_device(instance);

	std::println("Selected device: {}", physical_device.getProperties().deviceName.data());

	auto queue_family_index_ret = get_compute_queue_family_index(physical_device);

	if (!queue_family_index_ret)
	{
		std::println("{}", queue_family_index_ret.error());
		return ;
	}
	int queue_family_index = queue_family_index_ret.value();

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
	vk::Device device = physical_device.createDevice(device_info);
	std::string compiled_code = get_file_contents("../shaders/a.spv");
	vk::ShaderModuleCreateInfo shader_info(vk::ShaderModuleCreateFlags(), compiled_code.size(), reinterpret_cast<const uint32_t*>(compiled_code.c_str()));
	vk::ShaderModule shader_module = device.createShaderModule(shader_info);
    std::string compiled_code2 = get_file_contents("../shaders/tree.spv");
	vk::ShaderModuleCreateInfo shader_info2(vk::ShaderModuleCreateFlags(), compiled_code2.size(), reinterpret_cast<const uint32_t*>(compiled_code2.c_str()));
	vk::ShaderModule shader_module2 = device.createShaderModule(shader_info2);
    std::string compiled_code3 = get_file_contents("../shaders/bush.spv");
	vk::ShaderModuleCreateInfo shader_info3(vk::ShaderModuleCreateFlags(), compiled_code3.size(), reinterpret_cast<const uint32_t*>(compiled_code3.c_str()));
	vk::ShaderModule shader_module3 = device.createShaderModule(shader_info3);
    std::string compiled_code4 = get_file_contents("../shaders/biome.spv");
	vk::ShaderModuleCreateInfo shader_info4(vk::ShaderModuleCreateFlags(), compiled_code4.size(), reinterpret_cast<const uint32_t*>(compiled_code4.c_str()));
	vk::ShaderModule shader_module4 = device.createShaderModule(shader_info4);
    std::string compiled_code5 = get_file_contents("../shaders/surface.spv");
	vk::ShaderModuleCreateInfo shader_info5(vk::ShaderModuleCreateFlags(), compiled_code5.size(), reinterpret_cast<const uint32_t*>(compiled_code5.c_str()));
	vk::ShaderModule shader_module5 = device.createShaderModule(shader_info5);
    size_t size = (4096 * 24) * (64 * 64);
    auto buffer_out_ret_ret = create_buffer(device, physical_device, vk::BufferUsageFlagBits::eStorageBuffer, size);
    if (!buffer_out_ret_ret)
    {
        std::println("{}", buffer_out_ret_ret.error());
    }
    
    auto buffer_out_ret = buffer_out_ret_ret.value();
    vk::DeviceMemory buffer_out_memory = buffer_out_ret.first;
    vk::Buffer buffer_out = buffer_out_ret.second;
    
    
    size_t size_biome = (16 * 24) * (64 * 64);
    auto buffer_biome_out_ret_ret = create_buffer(device, physical_device, vk::BufferUsageFlagBits::eStorageBuffer, size_biome);
    if (!buffer_biome_out_ret_ret)
    {
        std::println("{}", buffer_biome_out_ret_ret.error());
    }
    
    auto buffer_biome_out_ret = buffer_biome_out_ret_ret.value();
    vk::DeviceMemory buffer_biome_out_memory = buffer_biome_out_ret.first;
    vk::Buffer buffer_biome_out = buffer_biome_out_ret.second;


    int8_t *data = (int8_t *)device.mapMemory(buffer_out_memory, 0, size);
    int8_t *data_biome = (int8_t *)device.mapMemory(buffer_biome_out_memory, 0, size_biome);
    
    const std::vector<vk::DescriptorSetLayoutBinding> descriptor_set_layout_binding = {
        {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    };
    vk::DescriptorSetLayoutCreateInfo descriptor_layout_info(vk::DescriptorSetLayoutCreateFlags(), descriptor_set_layout_binding);
    vk::DescriptorSetLayout descriptor_set_layout = device.createDescriptorSetLayout(descriptor_layout_info);
    std::vector<vk::DescriptorSetLayout> layouts(2, descriptor_set_layout);

    vk::PushConstantRange push_range(vk::ShaderStageFlagBits::eCompute, 0, sizeof(parameters));

    vk::PipelineLayoutCreateInfo pipeline_layout_info(vk::PipelineLayoutCreateFlags(), descriptor_set_layout, push_range);
    vk::PipelineLayout pipeline_layout = device.createPipelineLayout(pipeline_layout_info);
    vk::PipelineCache pipeline_cache = device.createPipelineCache(vk::PipelineCacheCreateInfo());

    vk::PipelineShaderStageCreateInfo pipeline_shader_info(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, shader_module, "main");
    vk::ComputePipelineCreateInfo compute_pipeline_info(vk::PipelineCreateFlags(), pipeline_shader_info, pipeline_layout);
    vk::Pipeline pipeline = device.createComputePipeline(pipeline_cache, compute_pipeline_info).value;

    vk::PipelineShaderStageCreateInfo pipeline_shader_info2(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, shader_module2, "main");
    vk::ComputePipelineCreateInfo compute_pipeline_info2(vk::PipelineCreateFlags(), pipeline_shader_info2, pipeline_layout);
    vk::Pipeline pipeline2 = device.createComputePipeline(pipeline_cache, compute_pipeline_info2).value;

    vk::PipelineShaderStageCreateInfo pipeline_shader_info3(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, shader_module3, "main");
    vk::ComputePipelineCreateInfo compute_pipeline_info3(vk::PipelineCreateFlags(), pipeline_shader_info3, pipeline_layout);
    vk::Pipeline pipeline3 = device.createComputePipeline(pipeline_cache, compute_pipeline_info3).value;

    vk::PipelineShaderStageCreateInfo pipeline_shader_info4(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, shader_module4, "main");
    vk::ComputePipelineCreateInfo compute_pipeline_info4(vk::PipelineCreateFlags(), pipeline_shader_info4, pipeline_layout);
    vk::Pipeline pipeline4 = device.createComputePipeline(pipeline_cache, compute_pipeline_info4).value;

    vk::PipelineShaderStageCreateInfo pipeline_shader_info5(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, shader_module5, "main");
    vk::ComputePipelineCreateInfo compute_pipeline_info5(vk::PipelineCreateFlags(), pipeline_shader_info5, pipeline_layout);
    vk::Pipeline pipeline5 = device.createComputePipeline(pipeline_cache, compute_pipeline_info5).value;

    vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 2);
    vk::DescriptorPoolCreateInfo descriptor_pool_info(vk::DescriptorPoolCreateFlags(), 2, descriptor_pool_size);
    vk::DescriptorPool descriptor_pool = device.createDescriptorPool(descriptor_pool_info);

    vk::DescriptorSetAllocateInfo descriptor_alloc_info(descriptor_pool, 2, layouts.data());
    std::vector<vk::DescriptorSet> descriptor_sets = device.allocateDescriptorSets(descriptor_alloc_info);
    vk::DescriptorBufferInfo buffer_out_info(buffer_out, 0, size);
    std::vector<vk::WriteDescriptorSet> write_descriptor_sets =
    {
        {descriptor_sets[0], 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &buffer_out_info}
    };
    device.updateDescriptorSets(write_descriptor_sets, {});

    vk::DescriptorBufferInfo buffer_biome_out_info(buffer_biome_out, 0, size_biome);
    std::vector<vk::WriteDescriptorSet> write_descriptor_sets_biome =
    {
        {descriptor_sets[1], 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &buffer_biome_out_info}
    };
    device.updateDescriptorSets(write_descriptor_sets_biome, {}); 
    
    vk::CommandPoolCreateInfo command_pool_info(vk::CommandPoolCreateFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer), queue_family_index);
    vk::CommandPool command_pool = device.createCommandPool(command_pool_info);

    vk::CommandBufferAllocateInfo command_buffer_alloc_info(command_pool, vk::CommandBufferLevel::ePrimary, 1);
    std::vector<vk::CommandBuffer> command_buffers = device.allocateCommandBuffers(command_buffer_alloc_info);

    vk::CommandBuffer command_buffer = command_buffers.front();
    vk::Fence fence = device.createFence(vk::FenceCreateInfo());
    vk::Queue queue = device.getQueue(queue_family_index, 0);
    
    while(thread == true)
	{
		std::unique_lock lock(world_mut);
		notify_send.wait_for(lock, std::chrono::milliseconds(5));
        if (chunks_to_send.empty() == false)
        {
            std::vector<chunk_request> positions = std::move(chunks_to_send);
            std::vector<chunk> ret;
            for (int i = 0; i < positions.size(); i++)
            {
                ret.emplace_back(0, 0, w.biomes);
            }
            chunks_to_send.clear();
            lock.unlock();
            using clock = std::chrono::system_clock;
            using ms = std::chrono::duration<double, std::milli>;

            const auto before = clock::now();
            const auto before3 = clock::now();
            vk::CommandBufferBeginInfo command_buffer_begin_info{vk::CommandBufferUsageFlags()};
            command_buffer.begin(command_buffer_begin_info);
            parameters params;
            
            command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline4);
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_sets[1]}, {});
            for (int c = 0; c < positions.size(); c++)
            {
                params.x = positions[c].x;
                params.z = positions[c].z;
                params.chunk = c;
                params.seed = (float)continentality_seed;
                
                command_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(parameters), &params);
                command_buffer.dispatch(1, 1, 1);
            }

            command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline5);
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_sets[0]}, {});
            for (int c = 0; c < positions.size(); c++)
            {
                params.x = positions[c].x;
                params.z = positions[c].z;
                params.chunk = c;
                params.seed = (float)continentality_seed;

                command_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(parameters), &params);
                command_buffer.dispatch(1, 1, 1);
            }


            command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_sets[0]}, {});
            for (int c = 0; c < positions.size(); c++)
            {
                params.x = positions[c].x;
                params.z = positions[c].z;
                params.chunk = c;
                params.seed = (float)continentality_seed;

                
                command_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(parameters), &params);
                command_buffer.dispatch(1, 1, 1);
            }

            command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline3);
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_sets[0]}, {});
            for (int c = 0; c < positions.size(); c++)
            {
                params.x = positions[c].x;
                params.z = positions[c].z;
                params.chunk = c;
                params.seed = (float)continentality_seed;

                command_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(parameters), &params);
                command_buffer.dispatch(1, 1, 1);
            }

            command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline2);
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_sets[0]}, {});
            for (int c = 0; c < positions.size(); c++)
            {
                params.x = positions[c].x;
                params.z = positions[c].z;
                params.chunk = c;
                params.seed = (float)continentality_seed;

                command_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(parameters), &params);
                command_buffer.dispatch(1, 1, 1);
                
            }
            command_buffer.end();
            vk::SubmitInfo submit_info(0, nullptr, nullptr, 1, &command_buffer);
            queue.submit({submit_info}, fence);
            auto res = device.waitForFences({fence}, true, -1);
            device.resetFences(fence);
            command_buffer.reset();
            const ms duration3 = clock::now() - before3;
            log(std::format("GPU compute took {}", duration3), LOG_LEVEL::NORMAL);
            
            const auto before2 = clock::now();
            for (int i = 0; i < positions.size(); i++)
            {
                chunk &c = ret[i];
                c.x = positions[i].x;
                c.z = positions[i].z;
                for (int x = 0; x < c.sections.size(); x++)
                {
                    int index = x * 4096 + (i * 98304);
                    std::memcpy(c.sections[x].blocks.data(), &data[index], 4096);
                    auto first_block = c.sections[x].blocks[0];
                    bool equal = true;
                    for (int b = 1; b < 4096; b++)
                    {
                        if (c.sections[x].blocks[b] != first_block)
                        {
                            equal = false;
                            break;
                        }
                    }

                    if (equal)
                    {
                        c.sections[x].blocks.resize(1); 
                        c.sections[x].blocks[0] = first_block;
                    }
                    int index_biome = x * 16 + (i * 384);
                    std::memcpy(c.sections[x].biome.data(), &data_biome[index_biome], 16);
                }
            }
            const ms duration2 = clock::now() - before2;
            log(std::format("Downloading took {}", duration2), LOG_LEVEL::NORMAL);
            const ms duration = clock::now() - before;
            log(std::format("Total took {} ({} chunks)", duration, positions.size()), LOG_LEVEL::NORMAL);
            log(std::format("That's {} per chunk", duration/positions.size()), LOG_LEVEL::NORMAL);  
            for (int i = 0; i < positions.size(); i++)
            {
                auto chunk_data = std::make_tuple(ret[i].x, ret[i].z, minecraft::varint(0), std::ref(ret[i]), minecraft::varint(0),
                            minecraft::varint(0),minecraft::varint(0),minecraft::varint(0),
                            minecraft::varint(0),minecraft::varint(0), minecraft::varint(0));
                sv.send_packet(chunk_data, positions[i].fd, 0x27);
            }
            
            memset(data, 0, (4097 * 24) * positions.size());
            memset(data_biome, 0, (16 * 24) * positions.size());
            positions.clear();
        }
	}
}