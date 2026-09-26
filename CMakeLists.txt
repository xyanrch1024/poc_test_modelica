cmake_minimum_required(VERSION 3.24)
project(modelicac LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# SimCode → C++ 模板引擎（header-only；嵌入 nlohmann/json）
set(INJA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INJA_USE_EMBEDDED_JSON ON CACHE BOOL "" FORCE)
set(INJA_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  inja
  URL https://github.com/pantor/inja/archive/refs/tags/v3.4.0.zip
)
FetchContent_MakeAvailable(inja)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
