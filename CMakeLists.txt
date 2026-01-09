cmake_minimum_required(VERSION 3.7)

project(NESEmu)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)

add_executable(nesemu)

target_link_libraries(nesemu PRIVATE SDL3::SDL3)

target_sources(nesemu
    PRIVATE
        src/Main.cpp

    PUBLIC
        FILE_SET headers
        TYPE HEADERS
        BASE_DIRS
            include
        FILES
            include/Emulator.hpp
)
