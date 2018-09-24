cmake_minimum_required(VERSION 3.9)
project(system_hw5 C)

set(CMAKE_C_STANDARD 11)
#set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_executable(system_hw5 main.c debug.h)
target_link_libraries(system_hw5 pthread)
target_link_libraries(system_hw5 m)