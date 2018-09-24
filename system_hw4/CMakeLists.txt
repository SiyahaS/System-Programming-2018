cmake_minimum_required(VERSION 3.9)
project(system_hw4 C)

set(CMAKE_C_STANDARD 99)

add_executable(system_hw4 main.c)
target_link_libraries(system_hw4 pthread rt)