cmake_minimum_required(VERSION 3.28)
project(The little Fuzzer)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)

add_executable(fuzzer
        fuzzer.cpp
        json.hpp)
