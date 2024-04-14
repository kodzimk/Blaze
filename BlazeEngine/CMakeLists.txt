cmake_minimum_required(VERSION 3.29.0)

file(GLOB LIB_HEADERS src/*.h)
file(GLOB LIB_SOURCES src/*.cpp)


add_executable(BlazeEngine ${LIB_SOURCES} ${LIB_HEADERS})