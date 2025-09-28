#pragma once

#include <filesystem>
#include <format>
#include "networking/comp_time_write.h"
#include "networking/mc_netlib.h"

int send_registry(int fd, server &sv);