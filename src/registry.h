#pragma once

#include <filesystem>
#include <format>
#include "../netlib_/src/networking.h"
#include "deserialize.h"

int send_registry(int fd, server &sv);
int get_registry(std::vector<std::string> &biomes, std::vector<std::string> &dimensions);