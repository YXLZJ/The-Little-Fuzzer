cmake_minimum_required(VERSION 3.28)
project(The_Little_Fuzzer)

set(CMAKE_CXX_STANDARD 20)

include_directories(.)

if("${method}" STREQUAL "vm")
        add_executable(vmfuzzer vm.cpp json.hpp)
else()
        add_executable(fuzzer fuzzer.cpp json.hpp)
endif()
