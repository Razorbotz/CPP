cmake_minimum_required(VERSION 3.1)
project(ground_plane_opencv)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PCL REQUIRED)
find_package(OpenCV REQUIRED)
find_package(realsense2 REQUIRED)

include_directories(${PCL_INCLUDE_DIRS})
add_definitions(${PCL_DEFINITIONS})

add_executable(ground_plane_opencv main.cpp)
target_link_libraries(ground_plane_opencv
    ${PCL_LIBRARIES}
    ${OpenCV_LIBS}
    realsense2::realsense2
)

