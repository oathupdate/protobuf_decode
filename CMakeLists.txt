cmake_minimum_required(VERSION 2.8)

project(decoder)

SET (CMAKE_CXX_FLAGS "-O0 -Wall -g -std=c++11 ")
set(DEPS_INCLUDE_DIR "${PROJECT_SOUC_DIR} ")
set(DEPS_LIBRARY_DIR "${PROJECT_OUCE_DI}/Lib ")

add_subdirectory(protobuf)
