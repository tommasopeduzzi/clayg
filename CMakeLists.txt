cmake_minimum_required(VERSION 3.24)
project(clayg)

set(CMAKE_CXX_STANDARD 17)

add_executable(clayg main.cpp DecodingGraph.cpp DecodingGraph.h Cluster.cpp Cluster.h UnionFindDecoder.cpp UnionFindDecoder.h PeelingDecoder.cpp PeelingDecoder.h ClAYGDecoder.cpp ClAYGDecoder.h Decoder.cpp Decoder.h)
