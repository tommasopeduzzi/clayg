cmake_minimum_required(VERSION 3.24)
project(clayg)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")

add_executable(clayg main.cpp DecodingGraph.cpp DecodingGraph.h Cluster.cpp Cluster.h UnionFindDecoder.cpp UnionFindDecoder.h PeelingDecoder.cpp PeelingDecoder.h ClAYGDecoder.cpp ClAYGDecoder.h Decoder.cpp Decoder.h Logger.cpp Logger.h)
