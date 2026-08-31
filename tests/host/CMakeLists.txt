cmake_minimum_required(VERSION 3.24)
project(gbb_dongle_host_tests LANGUAGES CXX)

option(GBB_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(GBB_ENABLE_COVERAGE "Enable gcov-compatible coverage instrumentation" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.3
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.8.1
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ArduinoJson Catch2)

set(GBB_COMPONENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../components/gbb_dongle")

add_library(gbb_core
  "${GBB_COMPONENT_DIR}/dongle_core.cpp"
  "${GBB_COMPONENT_DIR}/emergency_store.cpp"
  "${GBB_COMPONENT_DIR}/gbb_protocol.cpp"
  "${GBB_COMPONENT_DIR}/log_ring_buffer.cpp"
  "${GBB_COMPONENT_DIR}/modbus_executor.cpp"
)
target_include_directories(gbb_core PUBLIC "${GBB_COMPONENT_DIR}")
target_link_libraries(gbb_core PUBLIC ArduinoJson)
target_compile_options(gbb_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)

if(GBB_ENABLE_SANITIZERS)
  target_compile_options(gbb_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(gbb_core PUBLIC -fsanitize=address,undefined)
endif()

if(GBB_ENABLE_COVERAGE)
  target_compile_options(gbb_core PUBLIC --coverage -O0 -g)
  target_link_options(gbb_core PUBLIC --coverage)
endif()

add_executable(gbb_core_tests
  unit/test_protocol.cpp
  unit/test_modbus.cpp
  unit/test_emergency_store.cpp
  unit/test_log_ring_buffer.cpp
  unit/test_dongle_core.cpp
)
target_include_directories(gbb_core_tests PRIVATE unit)
target_link_libraries(gbb_core_tests PRIVATE gbb_core Catch2::Catch2WithMain)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)
add_executable(gbb_dongle_host
  runner/main.cpp
  runner/host_adapters.cpp
)
target_include_directories(gbb_dongle_host PRIVATE runner)
target_link_libraries(gbb_dongle_host PRIVATE gbb_core PkgConfig::MOSQUITTO)
target_compile_options(gbb_dongle_host PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
include(Catch)
catch_discover_tests(gbb_core_tests)
