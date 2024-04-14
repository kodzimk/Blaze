cmake_minimum_required(VERSION 3.29.0)

project(Blaze)

set(CMAKE_BUILD_TYPE Debug)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(CoreEngine/Blaze)

add_subdirectory(vendor/glfw)
target_link_libraries(CoreEngine PUBLIC glfw)

add_subdirectory(vendor/glew-cmake/build/cmake)
target_include_directories(CoreEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glew-cmake/out/include)
target_include_directories(CoreEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glew-cmake/out/src)
target_link_libraries(CoreEngine PUBLIC glew)

find_package(OpenGL REQUIRED)
target_link_libraries(CoreEngine PUBLIC OpenGL::GL)

target_include_directories(CoreEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/CoreEngine/Blaze/includes)

add_subdirectory(BlazeEngine)
target_link_libraries(BlazeEngine PUBLIC CoreEngine)