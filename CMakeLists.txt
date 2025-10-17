cmake_minimum_required(VERSION 3.5)
project(lily)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenSSL REQUIRED)

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
)

target_link_libraries(lily PRIVATE OpenSSL::Crypto)
