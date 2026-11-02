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

# Everything except main(), shared by the engine and the tests.
add_library(engine_core STATIC
  src/bitboard.cpp
  src/position.cpp
  src/movegen.cpp
)
target_include_directories(engine_core PUBLIC src)

if(MSVC)
  target_compile_options(engine_core PUBLIC /W4)
else()
  target_compile_options(engine_core PUBLIC -Wall -Wextra -Wpedantic)
  if(NATIVE)
    target_compile_options(engine_core PUBLIC -march=native)
  endif()
endif()

add_executable(engine src/main.cpp)
target_link_libraries(engine PRIVATE engine_core)

add_executable(perft_test tests/perft_test.cpp)
target_link_libraries(perft_test PRIVATE engine_core)

enable_testing()
add_test(NAME perft COMMAND perft_test)
