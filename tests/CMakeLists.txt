find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
  include(FetchContent)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.9.1
    GIT_SHALLOW TRUE
    SYSTEM)
  FetchContent_MakeAvailable(Catch2)
endif()

add_executable(unit-tests
  OptionsTest.cpp
  IntrospectionTest.cpp
  TextTest.cpp
  RateMeterTest.cpp
  ListCursorTest.cpp
)
target_link_libraries(unit-tests PRIVATE dbus-viewer-core dbus-viewer-warnings Catch2::Catch2WithMain)
add_test(NAME unit-tests COMMAND unit-tests)

# Needs a real bus: dbus-run-session starts a private one, isolated from the user's session.
add_executable(integration-tests CaptureTest.cpp)
target_link_libraries(integration-tests PRIVATE dbus-viewer-core dbus-viewer-warnings Catch2::Catch2WithMain)
find_program(DBUS_RUN_SESSION dbus-run-session)
if(DBUS_RUN_SESSION)
  add_test(NAME integration-tests COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:integration-tests>)
else()
  message(WARNING "dbus-run-session not found: integration tests will not run")
endif()
