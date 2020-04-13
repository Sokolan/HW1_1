cmake_minimum_required(VERSION 3.15)
project(untitled)

set(CMAKE_CXX_STANDARD 11)

add_executable(untitled smash.cpp signals.cpp Commands.cpp)