cmake_minimum_required(VERSION 3.24)
# 由 modelicac 0.1.0 生成。第三方依赖：无（仅标准库）。
# FR-002 政策：如需引入开源数学库，必须在此显式固定版本并声明来源。
project(Cooling LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
add_executable(Cooling model_Cooling.cpp)
target_include_directories(Cooling PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mcruntime)
