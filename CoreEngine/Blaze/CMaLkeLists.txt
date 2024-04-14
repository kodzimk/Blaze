cmake_minimum_required(VERSION 3.29.0)


file(GLOB LIB_HEADERS includes/*.h)
file(GLOB LIB_SOURCES src/*.cpp)

add_library(CoreEngine STATIC ${LIB_HEADERS} ${LIB_SOURCES})