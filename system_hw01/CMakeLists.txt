cmake_minimum_required(VERSION 3.9)
project(system_hw01 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer")

add_executable(system_hw01 main.c tiff.h tiff.c)