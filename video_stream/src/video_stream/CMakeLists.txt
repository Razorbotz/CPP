cmake_minimum_required(VERSION 3.5)
project(video_stream)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

# Add executable
add_executable(video_stream_node src/video_stream_node.cpp)

# Link libraries
ament_target_dependencies(video_stream_node
    rclcpp
    sensor_msgs
    cv_bridge
    image_transport
    OpenCV)

# Install the executable
install(TARGETS
    video_stream_node
    DESTINATION lib/${PROJECT_NAME})

# ament package
ament_package()

