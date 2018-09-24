cmake_minimum_required(VERSION 3.9)
project(system_hw02 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

add_executable(system_hw02 main.c)

target_link_libraries(system_hw02 m)