cmake_minimum_required(VERSION 3.10.0)
project(lily VERSION 0.1.0 LANGUAGES C CXX)

add_compile_options(-freflection -std=c++26)

add_subdirectory(netlib_)
find_package(OpenSSL REQUIRED)

add_executable(lily src/main.cpp src/mc_types.cpp
        src/packet.h
        src/deserialize.h
        src/log.cpp
        src/registry.cpp
        src/chunk.cpp
        src/chunk_send.cpp
        src/json_reader.cpp
        src/block_registry_processing.cpp
)
target_link_libraries(lily netlib OpenSSL::Crypto)

