cmake_minimum_required(VERSION 3.10)
set(CMAKE_LEGACY_CYGWIN_WIN32 0) 
project(control)

#set(CMAKE_C_COMPILER "/usr/bin/gcc.exe")
#set(CMAKE_CXX_COMPILER "/usr/bin/g++.exe")
#CMAKE_CXX_STANDARD(14)




include_directories(
	include
  /usr/include/glibmm-2.4 
  /usr/include/glibmm-2.4/glibmm
  /usr/lib/gktmm 
  /usr/lib/glibmm-2.4/include
  /usr/include/glib-2.0
  /usr/lib/glib-2.0/include
  /usr/include/sigc++-2.0/sigc++
  /usr/include/sigc++-2.0
  /usr/lib/sigc++-2.0/include
  /usr/include/giomm-2.4
  /usr/lib/giomm-2.4/include
  /usr/include/gdkmm-3.0
  /usr/lib/gdkmm-3.0/include
  /usr/lib/pangomm-1.4/include
  /usr/include/gtk-3.0
  /usr/include/pango-1.0
  /usr/include/cairo
  /usr/include/gdk-pixbuf-2.0
  /usr/include/cairomm-1.0
  /usr/include/freetype2 
  /usr/include/gtkmm-3.0
  /usr/lib/gtkmm-3.0/include
  /usr/include/pangomm-1.4/
  /usr/include/atkmm-1.6/
  /usr/include/atk-1.0

  /usr/lib/x86_64-linux-gnu/glibmm-2.4/include 
  /usr/lib/x86_64-linux-gnu/glib-2.0/include
  /usr/lib/x86_64-linux-gnu/sigc++-2.0/include
  /usr/lib/x86_64-linux-gnu/giomm-2.4/include
  /usr/lib/x86_64-linux-gnu/gdkmm-3.0/include
  /usr/lib/x86_64-linux-gnu/pangomm-1.4/include
  /usr/lib/x86_64-linux-gnu/gtkmm-3.0/include


  /usr/lib/aarch64-linux-gnu/glibmm-2.4/include 
  /usr/lib/aarch64-linux-gnu/glib-2.0/include
  /usr/lib/aarch64-linux-gnu/sigc++-2.0/include
  /usr/lib/aarch64-linux-gnu/giomm-2.4/include
  /usr/lib/aarch64-linux-gnu/gdkmm-3.0/include
  /usr/lib/aarch64-linux-gnu/pangomm-1.4/include
  /usr/lib/aarch64-linux-gnu/gtkmm-3.0/include
)


add_executable(control 
  src/control.cpp
  src/InfoFrame.cpp
  src/InfoItem.cpp
)


target_link_libraries(control
  SDL2
  glib-2.0
  gio-2.0
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
)

set_property(TARGET control PROPERTY CXX_STANDARD 17)
