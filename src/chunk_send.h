#pragma once
#include <vector>
#include "networking/mc_netlib.h"
#include "chunk.h"
#include <vulkan/vulkan.hpp>
#include <execution>

struct chunk_request
{
	chunk_request(int x_, int z_, int f)
	:x(x_), z(z_), fd(f)
	{}
	int x;
	int z;
	int fd;
};

void send_chunks(int fd, std::vector<std::pair<int, int>> &pos);
void world_thread(server &sv, world &w);