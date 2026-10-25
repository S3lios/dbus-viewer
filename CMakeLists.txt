cmake_minimum_required(VERSION 3.28)
project(dbus-viewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
pkg_check_modules(NCURSESW REQUIRED IMPORTED_TARGET ncursesw)

add_executable(dbus-viewer
  src/main.cpp
  src/Options.cpp
  src/Bus.cpp
  src/MessageRecord.cpp
  src/NameRegistry.cpp
  src/MessageStore.cpp
  src/Capture.cpp
  src/Introspector.cpp
  src/App.cpp
  src/ui/Terminal.cpp
  src/ui/ListCursor.cpp
  src/ui/StreamView.cpp
  src/ui/MemberView.cpp
  src/ui/ServiceView.cpp
  src/ui/TextOverlay.cpp
)

target_include_directories(dbus-viewer PRIVATE src)
target_compile_options(dbus-viewer PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
target_link_libraries(dbus-viewer PRIVATE PkgConfig::SYSTEMD PkgConfig::NCURSESW)

install(TARGETS dbus-viewer)
