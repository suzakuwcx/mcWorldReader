cmake_minimum_required(VERSION 3.28.3)

project(mcworldreader)

add_subdirectory(3rdparty)
add_subdirectory(src)

file(GLOB pysrc *.py)
