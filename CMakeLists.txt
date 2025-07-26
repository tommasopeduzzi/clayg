cmake_minimum_required(VERSION 3.24)
project(clayg)

set(CMAKE_CXX_STANDARD 17)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_executable(clayg
    main.cpp
    DecodingGraph.cpp DecodingGraph.h
    Cluster.cpp Cluster.h
    UnionFindDecoder.cpp UnionFindDecoder.h
    PeelingDecoder.cpp PeelingDecoder.h
    ClAYGDecoder.cpp ClAYGDecoder.h
    Decoder.cpp Decoder.h
    Logger.cpp Logger.h
)