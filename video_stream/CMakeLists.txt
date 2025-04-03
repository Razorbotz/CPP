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
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(WEBKITGTK REQUIRED webkit2gtk-4.0)

# Include directories
include_directories(
    include
    /usr/include/harfbuzz
    /cygwin64/usr/include/SDL2
    /usr/include/SDL2
    /usr/include/glib-2.0
    /usr/lib/glib-2.0/include
    /usr/include/glibmm-2.4 
    /usr/include/glibmm-2.4/glibmm
    /usr/lib/glibmm-2.4/include
    /usr/include/sigc++-2.0
    /usr/include/sigc++-2.0/sigc++
    /usr/lib/sigc++-2.0/include
    /usr/include/giomm-2.4
    /usr/lib/giomm-2.4/include
    /usr/include/pango-1.0
    /usr/include/pangomm-1.4/
    /usr/lib/pangomm-1.4/include
    /usr/include/cairo
    /usr/include/cairomm-1.0
    /usr/include/freetype2 
    /usr/include/gdkmm-3.0
    /usr/lib/gdkmm-3.0/include
    /usr/include/gdk-pixbuf-2.0
    /usr/include/gtk-3.0
    /usr/include/gtkmm-3.0
    /usr/lib/gktmm 
    /usr/lib/gtkmm-3.0/include
    /usr/include/atk-1.0
    /usr/include/atkmm-1.6/
  
    /usr/lib/aarch64-linux-gnu/glib-2.0/include
    /usr/lib/aarch64-linux-gnu/glibmm-2.4/include/
    /usr/lib/aarch64-linux-gnu/sigc++-2.0/include/
    /usr/lib/aarch64-linux-gnu/giomm-2.4/include/
    /usr/lib/aarch64-linux-gnu/pangomm-1.4/include/
    /usr/lib/aarch64-linux-gnu/gdkmm-3.0/include/
    /usr/lib/aarch64-linux-gnu/gtkmm-3.0/include/
  
    /usr/lib/arm-linux-gnueabihf/glib-2.0/include
    /usr/lib/arm-linux-gnueabihf/glibmm-2.4/include/
    /usr/lib/arm-linux-gnueabihf/sigc++-2.0/include/
    /usr/lib/arm-linux-gnueabihf/giomm-2.4/include/
    /usr/lib/arm-linux-gnueabihf/pangomm-1.4/include/
    /usr/lib/arm-linux-gnueabihf/gdkmm-3.0/include/
    /usr/lib/arm-linux-gnueabihf/gtkmm-3.0/include/
  
    /usr/lib/x86_64-linux-gnu/glib-2.0/include
    /usr/lib/x86_64-linux-gnu/glibmm-2.4/include
    /usr/lib/x86_64-linux-gnu/sigc++-2.0/include
    /usr/lib/x86_64-linux-gnu/giomm-2.4/include
    /usr/lib/x86_64-linux-gnu/pangomm-1.4/include
    /usr/lib/x86_64-linux-gnu/gdkmm-3.0/include
    /usr/lib/x86_64-linux-gnu/gtkmm-3.0/include
    ${GTKMM_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${WEBKITGTK_INCLUDE_DIRS}
    )

# Add executable
add_executable(video_stream_node 
    src/video_stream_node.cpp
    src/InfoFrame.cpp
    src/InfoItem.cpp
    src/BinaryMessage.cpp
    )

# Link libraries
target_link_libraries(video_stream_node
    ${rclcpp_LIBRARIES}
    ${sensor_msgs_LIBRARIES}
    ${cv_bridge_LIBRARIES}
    ${OpenCV_LIBRARIES}
    SDL2
    glib-2.0
    gio-2.0
    gobject-2.0
    giomm-2.4
    glibmm-2.4
    pango-1.0
    pangocairo-1.0
    gtk-3
    gtkmm-3.0
    sigc-2.0
    pthread
    gdk-3
    gdkmm-3.0
    atk-1.0
    atkmm-1.6
    ${GTKMM_LIBRARIES}
    ${CAIRO_LIBRARIES}
    cairo
    ${WEBKITGTK_LIBRARIES}
    )

# Install the executable
install(TARGETS
    video_stream_node
    DESTINATION lib/${PROJECT_NAME})

# ament package
ament_package()

