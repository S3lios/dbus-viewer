cmake_minimum_required(VERSION 3.28)

# Single source of truth for the version: CI requires a bump in every PR to main
# and tags the release from it.
project(dbus-viewer VERSION 1.0.0 LANGUAGES CXX)

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

add_library(dbus-viewer-warnings INTERFACE)
target_compile_options(dbus-viewer-warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)

# Everything but main(), so that tests link against the real code.
add_library(dbus-viewer-core STATIC
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
target_include_directories(dbus-viewer-core PUBLIC src)
target_compile_definitions(dbus-viewer-core PUBLIC DBUS_VIEWER_VERSION="${PROJECT_VERSION}")
target_link_libraries(dbus-viewer-core
  PUBLIC PkgConfig::SYSTEMD PkgConfig::NCURSESW
  PRIVATE dbus-viewer-warnings)

add_executable(dbus-viewer src/main.cpp)
target_link_libraries(dbus-viewer PRIVATE dbus-viewer-core dbus-viewer-warnings)

install(TARGETS dbus-viewer)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
