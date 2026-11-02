cmake_minimum_required(VERSION 3.16)
project(chess-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimised build: perft and search speed matter.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(NATIVE "Optimise for this machine's CPU (enables popcnt/BMI2 where available)" ON)

add_executable(engine
  src/main.cpp
)

if(MSVC)
  target_compile_options(engine PRIVATE /W4)
else()
  target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic)
  if(NATIVE)
    target_compile_options(engine PRIVATE -march=native)
  endif()
endif()
