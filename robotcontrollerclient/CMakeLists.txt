cmake_minimum_required(VERSION 3.12.0)
#set(CMAKE_LEGACY_CYGWIN_WIN32 0)
project(control)

#set(CMAKE_C_COMPILER "/usr/bin/gcc.exe")
#set(CMAKE_CXX_COMPILER "/usr/bin/g++.exe")
set(CMAKE_CXX_STANDARD 17)


find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
)

include_directories(
  include
  /usr/include/harfbuzz
  /cygwin64/usr/include/SDL2
  /usr/include/SDL2
  /usr/include/glib-2.0
  /usr/lib/glib-2.0/include
  /usr/include/glibmm-2.4 
  /usr/lib/glibmm-2.4/include
  /usr/include/sigc++-2.0
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
  ${OpenCV_INCLUDE_DIRS}
  ${FFMPEG_INCLUDE_DIRS}
)

include(FetchContent)

FetchContent_Declare(
  foxglove_websocket
  GIT_REPOSITORY https://github.com/foxglove/ws-protocol.git
  GIT_TAG main
)

FetchContent_GetProperties(foxglove_websocket)
if(NOT foxglove_websocket_POPULATED)
  FetchContent_Populate(foxglove_websocket)
  
  set(FOXGLOVE_WEBSOCKET_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(FOXGLOVE_WEBSOCKET_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  
  add_subdirectory(${foxglove_websocket_SOURCE_DIR}/cpp/foxglove-websocket ${foxglove_websocket_BINARY_DIR}/cpp/foxglove-websocket)
endif()

link_directories(
  include/
  ${GTKMM_LIBRARY_DIRS}
)

add_definitions(${GTKMM_CFLAGS_OTHER})

add_executable(control 
  src/control.cpp
  src/InfoFrame.cpp
  src/InfoItem.cpp
  src/BinaryMessage.cpp
  src/Speedometer.cpp
  src/ConfigDefinitions.cpp
  src/ConfigEditorWindow.cpp
  src/NetworkHandler.cpp
  src/BucketHeightIndicator.cpp
  src/BucketTiltIndicator.cpp
)


target_link_libraries(control
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
  ${OpenCV_LIBS}
  ${ZLIB_LIBRARIES}
  pthread
  CURL::libcurl
  PkgConfig::FFMPEG
  foxglove_websocket
  OpenSSL::SSL
  OpenSSL::Crypto
)

add_executable(input_rebind_tool
  src/InputRebindTool.cpp
)

target_include_directories(input_rebind_tool PRIVATE
  include
  ${GTKMM_INCLUDE_DIRS}
  ${CAIRO_INCLUDE_DIRS}
  ${foxglove_websocket_SOURCE_DIR}/cpp/foxglove-websocket/include
)

target_link_libraries(input_rebind_tool
  SDL2
  ${GTKMM_LIBRARIES}
  ${CAIRO_LIBRARIES}
  pthread
)