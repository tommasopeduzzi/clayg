cmake_minimum_required(VERSION 3.24)
project(clayg)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# --- Core library (shared code) ---
add_library(clayg_lib
        src/DecodingGraph.cpp
        src/Cluster.cpp
        src/UnionFindDecoder.cpp
        src/PeelingDecoder.cpp
        src/ClAYGDecoder.cpp
        src/Decoder.cpp
        src/Logger.cpp
        src/LogicalComputer.cpp
)

target_include_directories(clayg_lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --- Main executable ---
add_executable(clayg src/main.cpp)
target_link_libraries(clayg PRIVATE clayg_lib)

# -- Diagram Data Generator --
add_executable(diagram_data_generator tools/diagram_data_generator.cpp
        tools/diagram_data_generator.cpp)
target_link_libraries(diagram_data_generator PRIVATE clayg_lib)