project(scc)
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_CXX_FLAGS "-std=c++1y -Wall -Wextra -g -O2")

add_subdirectory(surface_mesh)
add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(tests)