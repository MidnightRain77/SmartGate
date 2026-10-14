cmake_minimum_required(VERSION 3.16)
project(stadium_entry LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SANITIZE "Build with a sanitizer: address | thread | undefined (empty = off)" "")

find_package(Threads REQUIRED)

add_library(stadium_core src/stadium.cpp src/simulation.cpp src/interactive.cpp)
target_include_directories(stadium_core PUBLIC include)
target_link_libraries(stadium_core PUBLIC Threads::Threads)
target_compile_options(stadium_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

add_executable(stadium src/main.cpp)
target_link_libraries(stadium PRIVATE stadium_core)

enable_testing()
add_executable(stadium_tests tests/tests.cpp)
target_link_libraries(stadium_tests PRIVATE stadium_core)
add_test(NAME unit COMMAND stadium_tests)

if(SANITIZE)
  foreach(t stadium_core stadium stadium_tests)
    target_compile_options(${t} PRIVATE -fsanitize=${SANITIZE} -g -fno-omit-frame-pointer)
    target_link_options(${t} PRIVATE -fsanitize=${SANITIZE})
  endforeach()
endif()
