cmake_minimum_required(VERSION 3.12.0)
project(control)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)


find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

include_directories(
  ${OpenCV_INCLUDE_DIRS}
  ${GTKMM_INCLUDE_DIRS}
)

link_directories(
  include/
  ${GTKMM_LIBRARY_DIRS}
)

add_definitions(${GTKMM_CFLAGS_OTHER})

add_executable(control
  src/control.cpp
)

target_link_libraries(control
  ${GTKMM_LIBRARIES}
  ${OpenCV_LIBS}
  SDL2
  pthread
)