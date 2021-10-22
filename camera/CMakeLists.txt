cmake_minimum_required(VERSION 2.8.3)
project(camera)

## Compile as C++11, supported in ROS Kinetic and newer
add_compile_options(-std=c++11)

include_directories(
  include
  glib-2.0
  /usr/include/glib-2.0
  /usr/lib/aarch64-linux-gnu/glib-2.0/include/
  /usr/include/gstreamer-1.0/
  gstreamer-1.0
)

add_executable(${PROJECT_NAME} 
	source/camera.cpp
)


target_link_libraries(${PROJECT_NAME}
  libgstreamer-1.0.so
  libgobject-2.0.so
  libglib-2.0.so
)

