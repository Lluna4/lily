cmake_minimum_required(VERSION 3.5)
project(lily)
link_directories("/opt/homebrew/lib")

set(CMAKE_CXX_STANDARD 23)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(lily src/main.cpp
        src/mc_types.cpp
        src/networking/epoll_utils.cpp
        src/networking/mc_netlib.cpp
        src/registry.cpp
        src/chunk.cpp
        src/block_registry_processing.cpp
        src/json_reader.cpp
        src/json_reader.h
        src/chat.cpp
        src/log.cpp
        src/blocks.cpp
        src/chunkgen.cpp
        src/world.cpp
)

target_link_libraries(lily PRIVATE OpenSSL::Crypto Threads::Threads Vulkan::Vulkan)
