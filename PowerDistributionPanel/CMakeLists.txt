# Project's name
cmake_minimum_required(VERSION 3.1)
project(talon C CXX)
#project(talon C CXX VERSION 1.0.0 DESCRIPTION "Test of CTR Software")
set(CMAKE_CXX_STANDARD 14)


set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)


include_directories("include")

add_executable(reader 
	src/reader.cpp
	src/PowerDistributionPanel.cpp	
)

target_link_libraries(reader
    libncurses.so
)
