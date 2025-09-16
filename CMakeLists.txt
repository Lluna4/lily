cmake_minimum_required(VERSION 4.0)
project(jelly)

set(CMAKE_CXX_STANDARD 23)

add_executable(jelly main.cpp
        mc_types.cpp
        comp_time_write.cpp
        epoll_utils.cpp
        mc_netlib.cpp
)
