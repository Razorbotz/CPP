# Project's name
cmake_minimum_required(VERSION 3.1)
project(talon C CXX)
#project(talon C CXX VERSION 1.0.0 DESCRIPTION "Test of CTR Software")
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-O0")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

include_directories("${PROJECT_SOURCE_DIR}/include")

add_executable(talon ${PROJECT_SOURCE_DIR}/talon.cpp)
target_link_libraries(talon
 libCTRE_Phoenix.a
 libCTRE_PhoenixCCI.a
 libCTRE_PhoenixPlatform_socketcan.a
 libCTRE_PhoenixCanutils.a
 libpthread.so
)
