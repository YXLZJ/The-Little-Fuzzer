cmake_minimum_required(VERSION 3.28)
project(The_Little_Fuzzer)

set(CMAKE_CXX_STANDARD 20)

include_directories(.)

set (COMMON_SRC
        fuzzer.cpp
        json.hpp)
# add_executable(fuzzer_${method} ${COMMON_SRC} )
# target_compile_definitions(fuzzer_${method} PRIVATE ${method}})
add_executable(fuzzer
        fuzzer.cpp
        json.hpp)
