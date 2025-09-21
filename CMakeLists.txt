cmake_minimum_required(VERSION 4.0)
project(lily)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenSSL REQUIRED)

add_executable(lily src/main.cpp
        src/mc_types.cpp
        src/networking/epoll_utils.cpp
        src/networking/mc_netlib.cpp
        src/registry.cpp
        src/registry.h
        src/chunk.cpp
        src/chunk.h
)

target_link_libraries(lily PRIVATE OpenSSL::Crypto)
