cmake_minimum_required(VERSION 3.9)
project(system_hw3)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DDEBUG)

add_executable(system_hw3 main.c tokenizer.c tokenizer.h reader.c reader.h dynamic.h dynamic.c debug.h)
add_executable(ls ls.c)
add_executable(cat cat.c)
add_executable(wc wc.c)